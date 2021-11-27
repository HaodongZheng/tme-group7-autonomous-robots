# TME290 Autonomous robots - Project

## Group 7 - Haodong Zheng & Josip Kir Hromatko
### Steps for testing in simulation environment by using online images
This repository contains the files needed for testing the course project. The instructions below show how to test the three tasks, but cone and Kiwi detection can be tested separately by following the instructions in their repositories.

* Step 1: The Docker images are available for deployment and the software can be tested simply by cloning this repository and running the desired `.yml` file:

```bash
cd $HOME
git clone https://git.chalmers.se/courses/tme290/2020/group7/tme290-group7-testing.git
```
* Step 2: Download the cone track scenario to $HOME in a terminal: 
```bash
wget https://raw.github.com/chalmers-revere/opendlv-tutorial-kiwi/master/conetrack.zip
```
* Step 3: Uncompress the zip file by running the command 
```bash
unzip conetrack.zip.
```
* Step 4: Next, in the terminal, run the command 
```bash
xhost +
``` 
to allow Docker to access the desktop environment (i.e. opening new windows on the display). This needs to be done once everytime you restart your computer.

* Step 5: Copy ceiling.jpg, cone.obj, floor.jpg, kiwi.obj, kiwi.png and wall.jpg into the conetrack/ and crossing2/ folder.
```bash
cd conetrack
cp ceiling.jpg cone.obj floor.jpg kiwi.obj kiwi.png wall.jpg ~/tme290-group7-testing/conetrack/
cp ceiling.jpg cone.obj floor.jpg kiwi.obj kiwi.png wall.jpg ~/tme290-group7-testing/crossing2/
``` 
* Step 6: Then start the corresponding -auto yml file to test each task in the simulation environment.
```bash
cd ~/tme290-group7-testing
docker-compose -f task-{1,2,3}-auto.yml
```

---

Instructions below show how to build the images locally and test the software.

### Task 1 - Navigating an empty cone track

* Step 1: Follow Step 1 to Step 5 in Steps for testing in simulation environment by using online images. 

* Step 2: Clone and build the `tme290-group7-cone-detection` image:
```bash
cd $HOME
git clone https://git.chalmers.se/courses/tme290/2020/group7/tme290-group7-cone-detection.git
docker build -t tme290-group7-cone-detection tme290-group7-cone-detection
```

* Step 3: Clone and build the `tme290-group7-logic-control` image:
```bash
cd $HOME
git clone https://git.chalmers.se/courses/tme290/2020/group7/tme290-group7-logic-control.git
docker build -t tme290-group7-logic-control tme290-group7-logic-control
```

* Step 4: Run the test using the provided `.yml` file:
```bash
cd tme290-group7-testing
docker-compose -f task-1.yml
```

---
### Task 2 - Navigating a cone track with other Kiwi cars

* Step 1-3: Follow Step 1 to Step 3 in Task 1 (if not done already). 

* Step 4: Clone and build the `tme290-group7-kiwi-detection` image:
```bash
cd $HOME
git clone https://git.chalmers.se/courses/tme290/2020/group7/tme290-group7-kiwi-detection.git
docker build -t tme290-group7-kiwi-detection tme290-group7-kiwi-detection
```

* Step 5: Run the test using the provided `.yml` file:
```bash
cd tme290-group7-testing
docker-compose -f task-2.yml
```

---
### Task 3 - Navigate a cone track with a crossing and other Kiwi cars

* Step 1-4: Follow steps 1, 2, 3 and 4 in Task 2(if not done already).

* Step 5: Run the test using the provided `.yml` file:
```bash
cd tme290-group7-testing
docker-compose -f task-3.yml
```
