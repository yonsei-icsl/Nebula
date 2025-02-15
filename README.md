# Nebula: Deep Neural Network Benchmarks in C++
Developed by Bogil Kim, Sungjae Lee, Chanho Park, Hyeonjin Kim, and William J. Song\
Intelligent Computing Systems Lab, Yonsei University\
Current release: v1.4.1 (Nov. 2020)

## Update 
We do not provide ImageNet large and medium datasets due to limited space quota of our cloud drive. If you want to execute DNN workloads with ImageNet large and medium datasets. Please download ImageNet dataset (ILSVRC2012) at the following link: 
https://image-net.org/challenges/LSVRC/2012/2012-downloads.php
After downloading training and validation images, it is required to move training datasets to train directory.

    $ mkdir train
    $ mv ILSVRC2012_img_train.tar train

And, unzip training and test datasets with the following command. 

    $ tar xf ILSVRC2012_img_val_v10102019.tar
    $ cd train/
    $ tar xf ILSVRC2012_img_train.tar

## Introduction
The rapid evolution of computing systems and explosive data production have driven significant advancements in machine learning, with neural networks being a cornerstone of emerging applications. However, as these networks grow deeper and larger to enhance accuracy and applicability, they pose significant challenges in modeling, simulation, and analysis due to the enormous computational and data processing demands. Motivated by these challenges, we developed Nebula, a neural network benchmark suite.

Nebula features a fully open-source implementation in C++, providing users with a transparent, efficient, and versatile framework. The benchmark suite currently includes seven representative neural networks – ResNet, VGG, AlexNet, MLP, DBN, LSTM, and RNN – with plans to expand the collection to include MobileNet, YOLO, Inception, and more. Inspired by popular benchmark suites like PARSEC and SPLASH-3, Nebula provides multiple dataset size options, ranging from large, full-fledged networks to medium and small proxies, offering flexibility for diverse experimental environments.

Initially, Nebula was designed to create lightweight neural network benchmarks that capture the key behaviors of neural networks while providing scalable options to suit various hardware and simulation needs at low computational overhead. However, the key appeal of the Nebula benchmark suite has evolved to be its robust C++ foundation and open-source accessibility, making it a valuable tool for users aiming to simulate, analyze, and experiment with neural networks effectively. Therefore, future updates to Nebula will expand its collection only with full-sized networks, removing lightweight proxies from its offerings.


## Prerequisites
Nebula uses g++ and nvcc to compile C++ codes to execute on CPUs and NVIDIA GPUs. It has dependency on OpenCV and OpenBLAS libraries to accelerate neural network algorithms. The Nebula benchmarks have been validated in 18.04 (Bionic Beaver) with g++-5.4, nvcc-9.0, OpenCV-3.2, and OpenBLAS-0.2 or any later versions of them (as of July, 2020).

    * g++-5.4 or later
    * nvcc-9.0 or later
    * OpenCV-3.2 or later
    * OpenBLAS-0.2 or later: optional for CPU acceleration
    * cuBLAS (of package nvidia-384 or later) and cuDNN (version 7.0.4 to 7.6.5): optional for GPU acceleration

In Ubuntu 18.04, use the following command to install the required libraries except for the NVIDIA driver package.

    $ sudo apt-get install build-essential g++ nvcc libopenblas-dev libopencv-dev

To install an NVIDIA driver package for cuBLAS and cuDNN libraries, refer to the following link: https://developer.nvidia.com/cuda-toolkit-archive. For example, installing an nvidia-384 package can be done by executing the following run command with sudo privilege.

    $ sudo ./cuda_9.0.176_384.81_linux-run

To install cuDNN libraries, refer to the following link:
https://docs.nvidia.com/deeplearning/cudnn/install-guide/index.html. For example, installing cudnn-v7.6.5 can be done by executing the following commands with sudo privilege.

    $ tar xf cudnn-10.2-linux-x64-v7.6.5.32.tgz
    $ sudo cp cuda/include/cudnn*.h /usr/local/cuda/include
    $ sudo cp cuda/lib64/libcudnn* /usr/local/cuda/lib64
    $ sudo chmod a+r /usr/local/cuda/include/cudnn*.h /usr/local/cuda/lib64/libcudnn*

## Download
The latest release of Nebula benchmark suite is v1.4.1 (as of Nov., 2020). To obtain a copy of Nebula v1.4,1, use the following git command in a terminal.

    $ git clone --branch v1.4.1 https://github.com/yonsei-icsl/nebula

Or, if you wish to use the latest development version, simply clone the git repository as is.

    $ git clone https://github.com/yonsei-icsl/nebula

## Build
Nebula provides a script file named nebula.sh to facilitate the build and run processes of benchmark suite. To build the entire models of Nebula suite, execute the script file as follows in the main directory of Nebula.

    $ cd nebula/
    $ ./nebula.sh build all

Alternatively, you may specify a benchmark of a particular size to build it by typing a command in the following format.

    $ ./nebula.sh build <network> <size>

For instance, a small benchmark of AlexNet can be built as follows. Possible options for the <network> and <size> fields are listed after the example.

    $ ./nebula.sh build alexnet small

Nebula v1.4.1 includes seven different types of neural networks, and each network has three different size options, i) large (L), medium (M), and small (S). The large benchmark of a given network type represents the full-fledged neural network, and the medium and small benchmarks are down-sized representations. Small benchmarks on average about 10-15x faster to run than full-fldged counterparts, while exhibiting similar hardware performance and characteristics. Medium benchmarks in general reduce the runtime by 3-5x with more similar emulation of full-fledged networks. The benchmarks have been rigorously validated across a variety of platforms including CPUs, GPUs, FPGAs, and NPUs. The following lists possible <network> and <size> options to put in the script run command shown above.

    Build command: ./nebula.sh build <network> <size>

    <network> options: alexnet, dbn, lstm, mlp, resnet, rnn, vgg
    <size> options: large, medium, small


## Configs, Dataset, and Pre-trained Weights
Executing a Nebula benchmark requires three inputs, i) network configs, ii) input dataset, and optionally iii) pre-trained weights. The network configs specify detailed structure of the neural network, and the config files should be found in the nebula/benchmarks/ directory after cloning from the github repository. Each sub-directory inside the nebula/benchmarks/ is named after a network type and size such as alexnet_small for small-sized benchmark of AlexNet.

A dataset is a group of input data consumed by the neural network. Nebula uses ImageNet for convolutional networks (i.e., AlexNet, ResNet, VGG), NIST for fully-connected networks (i.e., DBN, MLP), and PTB for recurrent networks (i.e., LSTM, RNN). These well-known datasets have been reformulated to fit for variable-sized Nebula benchmarks. Due to a limited space in the github repository to accommodate sizable datasets, Nebula maintains them in a remote Google Drive. To obtain a copy of a particular dataset, execute a script named dataset.sh in the following format.

    $ ./dataset.sh <dataset> <size>

For instance, a small-sized ImageNet can be obtained using the script as follows. Executing the script creates a directory named nebula/dataset/ (if it does not exist), and it places downloaded dataset files in the directory. Possible options for the <dataset> and <size> fields are listed after the example.

    $ ./dataset.sh imagenet small

The following shows possible <dataset> and <size> options to put in the script run command shown above.

    Dataset command: ./dataset.sh <dataset> <size>

    <dataset> options: imagenet, nist, ptb
    <size> options: large, medium, small

Nebula provides a set of pre-trained weights for user convenience. The weights can be used for inference of neural network benchmarks or optionally as initial states of training. Similar to the process of obtaining a dataset, weights can be downloaded using the weight.sh script file as follows.

    $ ./weight.sh <network> <size>

The following lists possible <network> and <size> options to put in the script run command shown above.

    Weight command: weight.sh <network> <size>

    <network> options: alexnet, dbn, lstm, mlp, resnet, rnn, vgg
    <size> options: large, medium, small


## Run
After a Nebula benchmark is built and dataset and weight files are downloaded, the benchmark becomes ready to execute either for inference or training. The nebula.sh script facilitates the execution of Nebula benchmark. A run command to execute the benchmark follows the format shown below. The <run type> field in the command specifies an execution type, i.e., test or train. The <benchmark> field indicates a target benchmark to run, such as vgg small for small-sized vgg.

    $ ./nebula.sh <run type> <benchmark> <small>

For example, the following command runs the inference of small-sized benchmark of VGG.

    $ ./nebula.sh test vgg small

Similarly, training a medium-sized ResNet can be done using the following command.

    $ ./nebula.sh train resnet medium


## Reference and Contact
To reference our work, please use our TC paper.

    @article{kim_tc2020,
        author  = {B. Kim and S. Lee and C. Park and H. Kim and W. Song},
        title   = {{The Nebula Benchmark Suite: Implications of Lightweight Neural Networks}},
        journal = {IEEE Transactions on Computers},
        volume  = {70},
        number  = {11},
        month   = {Nov.},
        year    = {2021},
        pages   = {1887-1900},
    }

For troubleshooting, bug reports, or any questions regarding the use of Nebula benchmark suite, please contact Bogil Kim via email: bogilkim {\at} yonsei {\dot} ac {\dot} kr. Or, visit our lab webpage: https://casl.yonsei.ac.kr
