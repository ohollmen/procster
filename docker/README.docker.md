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

## Running Registry (to mediate images)

Run registry (an container image) on localhost to mediate image transfer to final hosts:
```
# d =  detached, :2 =>  v2 API. Image persistence ONLY inside container !!!
docker run -d -p 5000:5000 --restart=always --name registry registry:2
# Test:
curl http://localhost:5000/v2/_catalog
{"repositories":[]}
# With image persistence on local filesystem (local chice of dir is arbitrary, but we use official path 1:1)
sudo mkdir -p /var/lib/registry
docker run -d -p 5000:5000 --restart=always --name registry -v /var/lib/registry:/var/lib/registry registry:2
```
Problems:
```
$ docker push host-01.lan:5000/procster:0.0.1-buildx
Using default tag: latest
The push refers to repository [host-01.lan:5000/procster]
Get "https://olli-B650M.lan:5000/v2/": http: server gave HTTP response to HTTPS client
# Solution: edit /etc/docker/daemon.json and add:
{
  "insecure-registries": ["host-01.lan:5000"]
}
# Retry:
docker push host-01.lan:5000/procster:0.0.1-buildx
The push refers to repository [host-01.lan:5000/procster]
fd2758d7a50e: Pushed 
0.0.1-buildx: digest: sha256:e9e9d51e25e4343f56b64d5ef1717234ec62241d93bf59734c53b4108b5c19ca size: 527
# Test (tag basename is considered to be a "repo")
curl http://localhost:5000/v2/_catalog
{"repositories":["procster"]}
```


Notes (on https and auth):
- For local development over HTTP, you may need to configure your Docker daemon to treat localhost:5000 as an "insecure-registry".
- For production environments or access from external hosts, you must configure the registry with valid
  SSL/TLS certificates and authentication (e.g., using htpasswd) to secure it over HTTPS.

Example /etc/docker/daemon.json
```
{
  "insecure-registries": ["host-01.lan:5000"],
  "builder": { "gc": { "defaultKeepStorage": "20GB", "enabled": true } },
  "experimental": false
}
```

Securing registry SSL + auth
```
# Use httpd:2 image as tool to create new user/passwd ents:
mkdir -p auth
docker run --entrypoint htpasswd httpd:2 -Bbn myuser mypassword > auth/htpasswd
# Use openssl 
mkdir -p certs
#openssl req -newkey rsa:4096 -nodes -sha256 -keyout certs/domain.key \
#  -x509 -days 365 -out certs/domain.crt -subj "/CN=host-01.lan"
# Cobtainer / GoLang tools fefuse CN Only, need SAN, With subjectAltName
openssl req -x509 -newkey rsa:4096 -sha256 -days 365 -nodes \
  -keyout certs/domain.key -out certs/domain.crt \
  -subj "/CN=host-01.lan" \
  -addext "subjectAltName=DNS:host-01.lan,IP:192.168.1.160"
# registy run Mounts and environment will grow !!!
# Note: was initially missing REGISTRY_AUTH_HTPASSWD_REALM (for htpasswd access controller) and REGISTRY_HTTP_SECRET
# TODO/Final: Create a config.yml and mount as /etc/docker/registry/config.yml
docker run -d -p 5000:5000 --name registry \
  -v /var/lib/registry:/var/lib/registry -v "$(pwd)"/auth:/auth -v "$(pwd)"/certs:/certs \
  -e REGISTRY_AUTH=htpasswd -e REGISTRY_AUTH_HTPASSWD_PATH=/auth/htpasswd \
  -e REGISTRY_HTTP_TLS_CERTIFICATE=/certs/domain.crt -e REGISTRY_HTTP_TLS_KEY=/certs/domain.key \
  -e REGISTRY_AUTH_HTPASSWD_REALM="Registry Realm" -e REGISTRY_HTTP_SECRET="a-long-random-string-here" \
  registry:2
# Note: config for registry is: /etc/docker/registry/config.yml

```
Copy your domain.crt to the host machine's /etc/docker/certs.d/host-01.lan:5000/ca.crt folder. Docker will then trust it specifically
(May need to do `sudo mkdir -p /etc/docker/certs.d/host-01.lan:5000` and `sudo cp -p certs/domain.crt /etc/docker/certs.d/host-01.lan:5000/ca.crt`).

Note: for `... build --push` step to work on a registry with self signed certificate, the (pushing) system cert trust needs to be updated:
```
# Copy the .crt to Ubuntu honored location
cp -p certs/domain.crt /usr/local/share/ca-certificates/
# Update certs (fast lookup cache ?)
sudo update-ca-certificates
# Restart Docker:
sudo systemctl restart docker
```
There is also a TOML config (buildkit.toml) way of updating trust at builder level:
```
# Create TOML config
[registry."B650M.lan:5000"]
  http = false
  insecure = true
# Run build 
docker buildx create --name mybuilder --config buildkitd.toml --driver docker-container --use --bootstrap
```

## Support for more architectures

Inspecting what architectures can be built:
```
docker buildx inspect --bootstrap
```

Installing all missing build architectures:
```
docker run --privileged --rm tonistiigi/binfmt --install all
```

Pulling linux/amd64 image from a remote registry (with cert trust steps taken, before that got errors:
- no such host => For some reason .lan domain is not recognized on host, add to /etc/hosts
- certificate signed by unknown authority => run trust steps (copy cert to this host)
- no basic auth credentials => `docker login host-01.lan:5000`
```
$ docker pull host-01.lan:5000/procster:0.0.2-buildx
0.0.2-buildx: Pulling from procster
no matching manifest for linux/arm64/v8 in the manifest list entries
```
Example manifest (to highlight how arch choices are laid out in there):
```
{
  "schemaVersion": 2,
  "mediaType": "application/vnd.docker.distribution.manifest.list.v2+json",
  "manifests": [
    {
      "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
      "size": 7143,
      "digest": "sha256:e692418a4d9b1f... (AMD64 version)",
      "platform": {
        "architecture": "amd64",
        "os": "linux"
      }
    },
    {
      "mediaType": "application/vnd.docker.distribution.manifest.v2+json",
      "size": 7632,
      "digest": "sha256:5b0bcabd1ed22... (ARM64 version)",
      "platform": {
        "architecture": "arm64",
        "os": "linux",
        "variant": "v8"
      }
    }
  ]
}
```

Manual Manifest Recreation:
```
# New manifest
docker manifest create your-repo/image:clean your-repo/image@sha256:e91475835fc01b593b7e97f73b47487c37c1a5e0e1ef15b589b7f8538feed5fb
# Annotate it (to ensure the architecture is explicitly set in the new index):
docker manifest annotate your-repo/image:clean your-repo/image@sha256:e91475835fc01b593b7e97f73b47487c37c1a5e0e1ef15b589b7f8538feed5fb --os linux --arch amd64
```
`docker buildx build ...` accepts a flag `--provenance=false` (Provenance Attestations)

## Attertations, provenance

See and read:
- Google (AI): what is provenance in docker context ?
  - Source Information: Git repo URL, commit SHA
  - Build Environment: Build system (GitHub Actions, BuildKit), platform (OS, Arch) and timestamps
  - Build Inputs: External materials: Base images, their digest
  - Build Process: exact steps or "invocation", full Dockerfile
  - Provenance (`--provenance=...`): 
- Build attestations: https://docs.docker.com/build/metadata/attestations/
- aligns with the SLSA (Supply-chain Levels for Software Artifacts) framework
- in-toto JSON format
