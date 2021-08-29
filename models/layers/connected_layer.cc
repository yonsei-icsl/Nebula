#include <algorithm>
#ifndef CUSTOM_BLAS
    #include <cblas.h>
#endif
#include <fstream>
#include <functional>
#include <cstring>
#include <random>
#include <thread>
#include "connected_layer.h"
#include "utils.h"
#include "batchnorm.h"
#include "gemm.h"

namespace nebula {

connected_layer_t::connected_layer_t(network_t *m_network, layer_t *m_prev_layer, layer_type_t m_layer_type) :
    layer_t(m_network, m_prev_layer, m_layer_type),
    bias(NULL),
    bias_update(NULL),
    weight_update(NULL),
    batch_normalize(false),
    scale(NULL),
    scale_update(NULL),
    normalize_mean(NULL),
    rolling_mean(NULL),
    mean_delta(NULL),
    normalize_variance(NULL),
    rolling_variance(NULL),
    variance_delta(NULL),
    x(NULL),
    normalize_x(NULL) {
}

connected_layer_t::~connected_layer_t() {
    delete [] bias;
    delete [] bias_update;
    delete [] weight;
    delete [] weight_update;
    delete [] output_data;
    delete [] delta;
    if(batch_normalize) {
        delete [] scale;
        delete [] scale_update;
        delete [] normalize_mean;
        delete [] rolling_mean;
        delete [] mean_delta;
        delete [] normalize_variance;
        delete [] rolling_variance;
        delete [] variance_delta;
        delete [] x;
        delete [] normalize_x;
    }
}

void connected_layer_t::init(section_config_t m_section_config) {
    // Get layer settings.
    m_section_config.get_setting("output", &output_size);
    m_section_config.get_setting("batch_normalize", &batch_normalize);  
    
    std::string activation_str;
    if(m_section_config.get_setting("activation", &activation_str)) {
        activation_type = (activation_type_t)get_type(activation_type_str, activation_str);
    }

    // Initialize layer parameters.
    input_size = prev_layer ? prev_layer->output_size : network->input_size;
    input_data = prev_layer ? prev_layer->output_data : network->input_data;
    weight_size = input_size * output_size;

    bias        = new float[output_size]();
    bias_update = new float[output_size]();

    weight        = new float[weight_size]();
    npu_mmu::npu_malloc((uint64_t)weight);
    weight_update = new float[weight_size]();

    output_data = new float[output_size * network->batch_size]();
    npu_mmu::npu_malloc((uint64_t)output_data);
    delta       = new float[output_size * network->batch_size]();

    if(batch_normalize) {
        scale        = new float[output_size]();
        scale_update = new float[output_size]();
        for(unsigned i = 0; i < output_size; i++) {
            scale[i] = 1.0;
        }

        normalize_mean = new float[output_size]();
        rolling_mean = new float[output_size]();
        mean_delta = new float[output_size]();

        normalize_variance = new float[output_size]();
        rolling_variance = new float[output_size]();
        variance_delta = new float[output_size]();

        x = new float[output_size * network->batch_size]();
        normalize_x = new float[output_size * network->batch_size]();
    }

}

// Initialize weight from weight file.
void connected_layer_t::init_weight(std::fstream &m_input_weight) {
    m_input_weight.read((char*)bias, output_size * sizeof(float));
    m_input_weight.read((char*)weight, weight_size * sizeof(float));

#ifdef PRUNING
    for(unsigned i = 0; i < weight_size; i++) {
        if(weight[i] < network-> weight_threshold && weight[i] > -network->weight_threshold) {
            weight[i] = 0.0;
        }
    }
#endif
    
    if(batch_normalize) {
        m_input_weight.read((char*)scale, output_size * sizeof(float));
        m_input_weight.read((char*)rolling_mean, output_size * sizeof(float));
        m_input_weight.read((char*)rolling_variance, output_size * sizeof(float));
    }
}

// Initialized weight from scratch.
void connected_layer_t::init_weight() {
    std::minstd_rand rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-1.0, 1.0);

    // Initialize weight 
    for(unsigned i = 0; i < weight_size; i++) {
        weight[i] = sqrt(2.0 / input_size) * dist(rng);
    }
}

void connected_layer_t::store_weight(std::fstream &m_output_weight) {
    m_output_weight.write((char*)bias, output_size * sizeof(float));
    m_output_weight.write((char*)weight, weight_size * sizeof(float));
    if(batch_normalize) {
        m_output_weight.write((char*)scale, output_size * sizeof(float));
        m_output_weight.write((char*)rolling_mean, output_size * sizeof(float));
        m_output_weight.write((char*)rolling_variance, output_size *sizeof(float));
    }
}

void connected_layer_t::forward() {
    memset(output_data, 0.0, output_size * network->batch_size * sizeof(float));
    memset(delta , 0.0, output_size * network->batch_size * sizeof(float));

#ifdef PRUNING
    unsigned zero_input = 0;
    for(unsigned i = 0; i < input_size; i++) {
        if(input_data[i] == 0.0) {zero_input++;}
    }
    std::cout << (float)zero_input/(float)input_size << " ";
    unsigned zero_weight = 0;
    for(unsigned i = 0; i < weight_size; i++) {
        if(weight[i] == 0.0) {zero_weight++;}
    }
    std::cout << (float)zero_weight/(float)weight_size << std::endl;
#endif
   
// Matrix multiplication
#ifdef CUSTOM_BLAS
    gemm(0, 1,
         network->batch_size, output_size, input_size,
         1.0,
         input_data, input_size,
         weight, input_size,
         1.0,
         output_data, output_size,
         num_threads);
#else
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, 
                network->batch_size, output_size, input_size, 
                1.0, 
                input_data, input_size, 
                weight, input_size, 
                1.0, 
                output_data, output_size);
#endif
    // Forward bias
    if(batch_normalize) {
        forward_batchnorm();
    } 
    forward_bias(num_threads, output_data, bias, output_size, 1, network->batch_size);
   
    // Activate function
    activate();

#ifdef PRUNING
    for(unsigned i = 0; i < output_size; i++) {

        if(output_data[i] < network->data_threshold && output_data[i] > -network->data_threshold) {
            output_data[i] = 0.0;
        }
    }
#endif
}

void connected_layer_t::forward(float *m_input_data) {
    memset(output_data, 0.0, output_size * network->batch_size * sizeof(float));
    memset(delta , 0.0, output_size * network->batch_size * sizeof(float));
   
    input_data = m_input_data ? m_input_data :   
                        prev_layer ? prev_layer->output_data : network->input_data;
// Matrix multiplication
#ifdef CUSTOM_BLAS
    gemm(0, 1,
         network->batch_size, output_size, input_size,
         1.0,
         input_data, input_size,
         weight, input_size,
         1.0,
         output_data, output_size,
         num_threads);
#else
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, 
                network->batch_size, output_size, input_size, 
                1.0, 
                input_data, input_size, 
                weight, input_size, 
                1.0, 
                output_data, output_size);
#endif
    // Forward bias
    if(batch_normalize) {
        forward_batchnorm();
    } 
    forward_bias(num_threads, output_data, bias, output_size, 1, network->batch_size);
   
    // Activate function
    activate();

}

void connected_layer_t::backward() {
    // Gradient function
    gradient();

    // Backward bias
    backward_bias(num_threads, bias_update, delta, output_size, 1, network->batch_size);
    if(batch_normalize) {
        backward_batchnorm();
    } 
    
    input_data = prev_layer ? prev_layer->output_data : network->input_data;
    float *prev_delta = prev_layer ? prev_layer->delta : NULL;

#ifdef CUSTOM_BLAS
    gemm(1, 0,
         output_size, input_size, network->batch_size,
         1.0,
         delta, output_size,
         input_data, input_size,
         1.0,
         weight_update, input_size,
         num_threads);
#else
    // Matrix multiplication for weight update
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans, 
                output_size, input_size, network->batch_size, 
                1.0, 
                delta, output_size, 
                input_data, input_size, 
                1.0, 
                weight_update, input_size);
#endif

    // Matrix multiplication for delta update
    if(prev_delta) {
#ifdef CUSTOM_BLAS
        gemm(0, 0,
             network->batch_size, input_size, output_size,
             1.0,
             delta, output_size,
             weight, input_size,
             1.0,
             prev_delta, input_size,
             num_threads);
#else
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, 
                    network->batch_size, input_size, output_size, 
                    1.0, 
                    delta, output_size, 
                    weight, input_size, 
                    1.0, 
                    prev_delta, input_size);
#endif
    }
}

void connected_layer_t::backward(float *m_input_data, float *m_delta){

    input_data = m_input_data ? m_input_data :
                        prev_layer ? prev_layer->output_data : network->input_data;
    float *prev_delta = m_delta ? m_delta :
                        prev_layer ? prev_layer->delta : 0;
    // Gradient function
    gradient();


    // Backward bias
    backward_bias(num_threads, bias_update, delta, output_size, 1, network->batch_size);
    if(batch_normalize) {
        backward_batchnorm();
    } 

#ifdef CUSTOM_BLAS
    gemm(1, 0,
         output_size, input_size, network->batch_size,
         1.0,
         delta, output_size,
         input_data, input_size,
         1.0,
         weight_update, input_size,
         num_threads);
#else
    // Matrix multiplication for weight update
    cblas_sgemm(CblasRowMajor, CblasTrans, CblasNoTrans, 
                output_size, input_size, network->batch_size, 
                1.0, 
                delta, output_size, 
                input_data, input_size, 
                1.0, 
                weight_update, input_size);
#endif

    // Matrix multiplication for delta update
    if(prev_delta) {
#ifdef CUSTOM_BLAS
        gemm(0, 0,
             network->batch_size, input_size, output_size,
             1.0,
             delta, output_size,
             weight, input_size,
             1.0,
             prev_delta, input_size,
             num_threads);
#else
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, 
                    network->batch_size, input_size, output_size, 
                    1.0, 
                    delta, output_size, 
                    weight, input_size, 
                    1.0, 
                    prev_delta, input_size);
#endif
    }

}

void connected_layer_t::update() {
#ifdef CUSTOM_BLAS
    axpy(output_size, network->learning_rate / network->batch_size, bias_update, 1, bias, 1);
    scal(output_size, network->momentum, bias_update, 1);

    axpy(weight_size, -network->decay * network->batch_size, weight, 1, weight_update, 1);
    axpy(weight_size, network->learning_rate / network->batch_size, weight_update, 1, weight, 1);
    scal(weight_size, network->momentum, weight_update, 1);

    if(batch_normalize) {
        axpy(output_size, network->learning_rate / network->batch_size, scale_update, 1, scale, 1);
        scal(output_size, network->momentum, scale_update, 1);
    }
#else
    //Update bias.
    cblas_saxpy(output_size, network->learning_rate / network->batch_size, bias_update, 1, bias, 1);
    cblas_sscal(output_size, network->momentum, bias_update, 1);

    //Update weight.
    cblas_saxpy(weight_size, (0.0 - network->decay) * network->batch_size, weight, 1, weight_update, 1);
    cblas_saxpy(weight_size, network->learning_rate / network->batch_size, weight_update, 1, weight, 1);
    cblas_sscal(weight_size, network->momentum, weight_update, 1);
    
    if(batch_normalize) {
        cblas_saxpy(output_size, network->learning_rate/network->batch_size, scale_update, 1, scale, 1);
        cblas_sscal(output_size, network->momentum, scale_update, 1);
    }
#endif
}

// Forward batch normalization.
void connected_layer_t::forward_batchnorm() {
    memcpy(x, output_data, output_size * network->batch_size * sizeof(float));
    if(network->run_type == TRAIN_RUN) {
        batchnorm_mean(num_threads, output_data, normalize_mean, 
                       output_size, 1, network->batch_size);
        batchnorm_variance(num_threads, output_data, normalize_mean, normalize_variance, 
                           output_size, 1, network->batch_size);
#ifdef CUSTOM_BLAS
        scal(output_size, 0.99, rolling_mean, 1);
        axpy(output_size, 0.01, normalize_mean, 1, rolling_mean, 1);
        scal(output_size, 0.99, rolling_variance, 1);
        axpy(output_size, 0.01, normalize_variance, 1, rolling_variance, 1);
#else
        cblas_sscal(output_size, 0.99, rolling_mean, 1);
        cblas_saxpy(output_size, 0.01, normalize_mean, 1, rolling_mean, 1);
        cblas_sscal(output_size, 0.99, rolling_variance, 1);
        cblas_saxpy(output_size, 0.01, normalize_variance, 1, rolling_variance, 1);
#endif
        // Normalize all output data.
        batchnorm_normalize(num_threads, output_data, normalize_mean, normalize_variance, 
                            output_size, 1, network->batch_size);
        memcpy(normalize_x, output_data, output_size*network->batch_size*sizeof(float));
    }
    else {
        // normalize all output data.
        batchnorm_normalize(num_threads, output_data, rolling_mean, rolling_variance, 
                            output_size, 1, network->batch_size);
    }
    batchnorm_scale_down(num_threads, output_data, scale, 
                         output_size, 1, network->batch_size);
}

//Backward batch normalization.
void connected_layer_t::backward_batchnorm() {
    //scale_bias(num_threads, delta, scale, output_size, output_height * output_width, network->batch_size); 
    batchnorm_scale_down(num_threads, delta, scale, 
                         output_size, 1, network->batch_size);
    batchnorm_mean_delta(num_threads, delta, normalize_variance, mean_delta, 
                          output_size, 1, network->batch_size);
    batchnorm_variance_delta(num_threads, x, delta, 
                             normalize_mean, normalize_variance,  variance_delta, 
                             output_size, 1, network->batch_size);
    batchnorm_normalize_delta(num_threads, x, normalize_mean, normalize_variance, 
                              mean_delta, variance_delta, delta, 
                              output_size, 1, network->batch_size); 
}

void connected_layer_t::increment(int step){
    int num = (int)output_size * (int)network->batch_size * step;
    output_data += num; 
    delta += num;
    
    if(batch_normalize) {
        x += num;
        normalize_x += num; 
    }
}

}
// End of namespace nebula.
