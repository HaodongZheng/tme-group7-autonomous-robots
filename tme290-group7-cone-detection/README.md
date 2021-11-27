# Cone detection using OpenCV

## Building and testing the software module (replay mode)

* Step 1: Assuming that you have a folder `~/kiwi-recordings`, where you have at least one `.rec` file from your experiments with Kiwi.

* Step 2: Clone this repository (if you haven't done so already):
```bash
cd $HOME
git clone https://git.chalmers.se/courses/tme290/2020/group7/tme290-group7-cone-detection.git
cd tme290-group7-cone-detection
```

* Step 3: Next, enable access to your X11 server (GUI; necessary once per login):
```bash
xhost +
```
Now, you can just run the `cone-detection-auto.yml` file, which will use the online registry. Remember to change the filename to a file that exists in `~/kiwi-recordings`.
```bash
docker-compose -f cone-detection-auto.yml up
```
The application should start and wait for images to come in. It will display detected red, yellow and blue cones.

---

If you would instead like to build the image locally or make modifications, do the following:

* Step 4: Build the software module as follows:
```bash
docker build -t tme290-group7-cone-detection .
```

* Step 5: Now, you can start the replay and the program as follows (the h264 replay is built once during the first call).  The replay will start automatically when the program starts, including a video stream put in shared memory.
```bash
docker-compose -f cone-detection.yml up
```

You can stop your software component by pressing `Ctrl-C`. When you are modifying the software component, repeat step 4 and step 5 after any change to your software.

After a while, you might have collected a lot of unused Docker images on your machine. You can remove them by running:
```bash
for i in $(docker images|tr -s " " ";"|grep "none"|cut -f3 -d";"); do docker rmi -f $i; done
```
