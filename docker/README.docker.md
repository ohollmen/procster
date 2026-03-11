# Procster as docker image/container

## Building Image

Building **procster** is facilitated as a 2-stage build with:
- First "build stage" downloading build tools (make, gcc) and development header files (*.h) and libraries
- Second stage copying only essential binaries from build stage and installing only the essential
  runtime (required) libraries to second stage
  
```
# On the top of the source tree
docker build --rm=true -t procster:0.0.1 -f docker/Dockerfile .
```

## Running Container

```
docker run --rm -p 8181:8181  --pid=host 'procster:0.0.1'
```
# TODO

- Create a Kuberneters config to run procster as DaemonSet (on all nodes of cluster
  https://kubernetes.io/docs/concepts/workloads/controllers/daemonset/)
