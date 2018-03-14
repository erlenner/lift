#Lift

##Portability:
Targets Ubuntu 16. Works on Linux distributions which supports the external dependencies which are installed by [bootstrap.bash](scripts/bootstrap.bash).

##Prerequisites:
Install tup and g++-6. This can be accomplished through running `./scripts/bootstrap.bash`, or by running `./scripts/getTup.sh` and `./scripts/getg++-6.sh` separately.
If you provide an integer as parameter to bootstrap.bash, it will run the lift with the same parameter after everything is installed and built.

##Build:
Run `tup`.

##Run:
Run `./build-native/src/lift/run <id>`. Replace `<id>` with the id of the lift (0, 1, 2, ...).

##Restart on crash:
If you run `./scripts/keepAlive.bash <id>` the lift will be executed with the id you specify in `<id>` and it will be restarted every time it crashes (when it stops with a non-zero exit value).

##Restart on reboot
Making the lift restart on reboot is up to the user. One option is to edit your crontab by running `crontab -e` and then place the path of the lift executable in it by adding the following line: `@reboot <absolute-path-of-lift-executable>`.
If it's supposed to run on every boot, it is customary to run it from a shell script in `/etc/init.d/` and adding the script by running `update-rc.d <script-name> <command-line-options>`.

##Memory
The lift will keep record of its own and the other lifts' states in the file `build-native/src/lift/state_save.cereal` so that it can continue serving any orders it didn't finish when it is restarted.

#Functionality
The lifts are equal peers and they broadcasts their own state and listens to the others broadcasts.

Tasks are divided into pending tasks, which aren't being served, and plain tasks, which are currently being served by the relevant lift.

When there are new pending tasks, each lift will calculate it's FS (Figure of Suitability), which is a score with respect to the task.
If a lift finds it has the lowest FS, it will take the task, moving it into it's "tasks" list.
If it finds that another lift with a lower id has also taken the task, it will remove the task from its "tasks" list.

If a lift finds that there are other lifts with lower FS, it will not take the task, but it will keep track of how long the task has been pending and increase the margin associated with that pending task accordingly.
The lift will then keep comparing the FS's of the lifts, and if it reaches a point where the difference between the lowest FS and its own FS is less than the margin associated with the pending task, it will take the task anyway.

When a lift doesn't respond for too long, it will be registered as disconnected, and it's external tasks will be made pending again.

#Notes on code structure
General functions and structs without namespace prefixes are either found in the same file, in a file dedicated only to the struct, or in utils.h. All functions and classes dedicated to dealing with mutexes are found in mutexctrl.h.


#Dependency graph
![alt text](dependency-graph.png "Dependency graph")
