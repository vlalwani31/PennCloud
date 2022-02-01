# Package Installation

This project uses gRPC and Protocol Buffers to set up backend frontend communication.
Please follow the steps in [this tutorial](https://grpc.io/docs/languages/cpp/quickstart/) to install all necessary packages.
The install directory is default directory (`$HOME/.local`).

> Note:
> If you got stuck in `make -j` step, add number to constraint the resources. For example:
>
> ```
> $ make -j 3
> ```

## Include libraries

Use boost library for splitting strings

```
$ sudo apt-get install libboost-dev
```

# Create code repo

Clone the code repository inside the grpc folder that you created in the first step

```
$ cd {YOUR_GRPC_FOLDER}/examples/cpp
$ git clone ssh://T11@cis505.cis.upenn.edu/git/cis505/T11 ./T11
```

# Build project

### Check environment variable

```
$ echo $MY_INSTALL_DIR
```

If it is empty, then reset the environment variable:

```
$ export MY_INSTALL_DIR=$HOME/.local
$ export PATH="$MY_INSTALL_DIR/bin:$PATH"
```

> Note:
> This has to be executed everytime you start a terminal session

### Build gRPC

Change to the project directory

```
$ cd T11
```

Run `cmake`

```
$ mkdir -p cmake/build
$ cd cmake/build
$ cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..
```

### Generating client and server code

```
$ make kvstore.grpc.pb.o
```

Build the frontend and backend servers:

```
$ make
```

## How to run the code
### Run via script

Install xterm in the vm and run the following directly in the vm (not through an ssh session)
```
$ chmod u+x ../../script.sh
```

From the cmake/build directory, run
```
$ ./../../script.sh
```

You can also start backend and frontend servers manually through the following steps:
### Invoke backend

Invoke all the backend servers on config file.

```
$ ./key_value_store_server <backend server config file> <server id num>
```

For example:

```
$ ./key_value_store_server ../../config.txt 3
```

Start the backend master:

```
$ ./key_value_store_master ../../config.txt
```

### Invoke frontend

Start the frontend master:

```
$ ./frontend_master ../../config_front.txt 0
```

Invoke all the frontend servers on config file.

```
$ ./webserver <frontend server config file> <server id num>
```

For example:

```
$ ./webserver ../../config_front.txt 1
```
