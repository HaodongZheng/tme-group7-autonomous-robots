# Detection of Kiwi cars using OpenCV and YOLOv3-tiny

## Building and testing the software module (replay mode)

* Step 1: Assuming that you have a folder `~/kiwi-recordings`, where you have at least one `.rec` file from your experiments with Kiwi.

* Step 2: Clone this repository (if you haven't done so already):
```bash
cd $HOME
git clone https://git.chalmers.se/courses/tme290/2020/group7/tme290-group7-kiwi-detection.git
cd tme290-group7-kiwi-detection
```

* Step 3: Next, enable access to your X11 server (GUI; necessary once per login):
```bash
xhost +
```
Now, you can just run the `kiwi-detection-auto.yml` file, which will use the online registry. Remember to change the filename to a file that exists in `~/kiwi-recordings`.
```bash
docker-compose -f kiwi-detection-auto.yml up
```
The application should start and wait for images to come in. It will use the files provided in the `yolo` folder to detect Kiwi cars. It will output a message containing the top left corner coordinates and the dimensions of the detection's bounding box.

---

If you would instead like to build the image locally or make modifications, do the following:

* Step 4: Build the software module as follows:
```bash
docker build -t tme290-group7-kiwi-detection .
```

* Step 5: Now, you can start the replay and the program as follows (the h264 replay is built once during the first call). Remember to change the filename to a file that exists in `~/kiwi-recordings`. The replay will start automatically when the program starts, including a video stream put in shared memory.
```bash
docker-compose -f kiwi-detection.yml up
```

You can stop your software component by pressing `Ctrl-C`. When you are modifying the software component, repeat step 4 and step 5 after any change to your software.

After a while, you might have collected a lot of unused Docker images on your machine. You can remove them by running:
```bash
for i in $(docker images|tr -s " " ";"|grep "none"|cut -f3 -d";"); do docker rmi -f $i; done
```

<!---
---

## Deploying and testing the C++ application on Kiwi

When you are ready to test the features and performance of your software component on Kiwi in live mode, you need to build the software component for `armhf`. Therefore, you will find a file named `Dockerfile.armhf` in this template folder that describes the necessary steps to build your software component for `armhf`.  

* Step 1: Have the previous tutorial completed.

* Step 2: **Make sure that you commented all debug windows from OpenCV as there won't be a GUI available on Kiwi.''

* Step 3: Assuming that you are located in the `opendlv-perception-helloworld-cpp` folder, you can build the software module for `armhf` as follows:
```bash
docker build -t myapp.armhf -f Dockerfile.armhf .
```

* Step 4: After having successfully built the software component and packaged it into a Docker image for `armhf`, you need to transfer this Docker image from your laptop to Kiwi. Therefore, you save the Docker image to a file:
```bash
docker save myapp.armhf > myapp.armhf.tar
```

* Step 5: Next, you copy the image to Kiwi's *Raspberry Pi* using secure copy (`scp`):
```bash
scp -P 2200 myapp.armhf.tar pi@192.168.8.1:~
```

* Step 6: Afterwards, you log in to Kiwi's *Raspberry Pi* and load the Docker image:
```Bash
ssh -p 2200 pi@192.168.8.1
cat myapp.armhf.tar | docker load
```

* Step 7: Assuming that the Getting Started Tutorial 2 (Controlling Kiwi using your web browser) is still running, you can finally run your software component next to other microservices on Kiwi's *Raspberry Pi*:
```bash
docker run --rm -ti --init --net=host --ipc=host -v /tmp:/tmp myapp.armhf --cid=112 --name=img.argb --width=640 --height=480
```

Alternatively, you can also modify a `.yml` file from the Getting Started tutorial to include your software component:
```yml
    myapp:
        container_name: myapp
        image: myapp.armhf
        restart: on-failure
        network_mode: "host"
        ipc: "host"
        volumes:
        - /tmp:/tmp
        command: "--cid=112 --name=img.argb --width=640 --height=480"
```
---!>
