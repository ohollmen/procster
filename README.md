# Procster - List OS Processes over the Web

Procster is a simple microhttpd based HTTP service that lists
processes via a REST/JSON Web API.

Procster is meant to serve you in a toolkit / API manner, so that
you merely tap on to it with your favorite HTTP client and decide what
you do with data.
<!--, but Procster also ships with a small test-nature Web
GUI to graphically get hang of what data Procsster can provide.
-->

## Procster Design Rationale

Procster is written on top of 4 main, reusable footings:
- procps - Linux process lister API, also used by "ps" and "top" utilities
- microhttpd - Lightweight, standalone HTTP server
- glib - For helper data structures
- libjansson - JSON library to parse and serialize JSON

Choosing a very slim "embedded" web server (Instead of general purpose
web server like Apache or NginX) and efficient libraries for other tasks
keeps the memory footprint on the system "unnoticable".
As such Procster should be easily runnable on SBC systems like RaspberryPi,
Beaglebone, Odroid, etc.

## Procster and security

Typically listing OS processes is not someting you want to do in the world
of hacks and cracks and security breaches. This makes Procster mainly an
intranet, home or closed network tool.
See additional config possibilities of using HTTPS, authenication (HTTP
Basic Auth or API Key).

## Compiling, Running and Testing

Clone source from Github:
```
git clone https://github.com/ohollmen/procster.git
cd procster
```
Install dependencies (Example on Ubuntu 18.04)
```
# For Processlister ... (procps 3.3)
sudo apt-get install libprocps6 libpropcs-dev libjansson4 libjansson-dev
sudo apt-get install libglib2.0-dev
# For Microhttpd
sudo apt-get install libmicrohttpd12 libmicrohttpd-dev
```
... And Centos/RH (7.6)
```
# procps 3.3, jansson 2.10
sudo yum install -y procps-ng procps-ng-devel jansson jansson-devel
# TODO: glib2-static
sudo yum install -y glib2 glib2-devel
sudo yum install -y libmicrohttpd libmicrohttpd-devel
```

Compile libraries and process server:

```
make
```
Test CLI Utility output (No HTTP involved):
```
# Compact JSON
./proctest
# Prettified JSON
./proctest | python -m json.tool | less
```
Run HTTP/REST/JSON Process server (pass port as arg):
```
procserver 8181
```
Test HTTP Output:
```
# As-is compact JSON
curl http://localhost:8181/procs
# Prettified JSON, Inspect with pager
curl http://localhost:8181/procs | python -m json.tool | less
```
