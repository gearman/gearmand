gearmand Docker Images
======================

The Dockerfiles under the ```testing``` subdirectory are intended to be used to test building gearmand on various Linux distributions. They also quasi-document the prerequisite packages that need to be installed on these same Linux distributions in order to build gearmand from source.

The ```example``` gearmand Docker image can be built like this:

    cd example/supervisord/ && make latest && cd ../gearmand/ && make latest

The ```testing``` gearmand Docker images can be built like this, e.g.:

    cd testing/alpine/3.12.0 && make

Using SSL
---------

In order to use SSL with the ```example``` image, follow these steps before building the image using the above command:

1. Copy your certificate files to the ```gearmand``` subdirectory:

        cp /path/to/your/certificates/your-certificate-authority.pem example/gearmand/gearmand-ca.pem
        cp /path/to/your/certificates/gearmand.pem example/gearmand/gearmand.pem
        cp /path/to/your/certificates/gearmand.key example/gearmand/gearmand.key

   Or, better yet, use [Docker secrets](https://docs.docker.com/engine/swarm/secrets/).

2. Edit ```example/gearmand/Dockerfile``` and uncomment the line that copies these files to ```/var/lib/gearman/```.

3. Edit ```example/gearmand/gearmand.conf``` and uncomment the ```command``` line with the SSL-related arguments. Delete (or comment out) the ```command``` line on the next line in that file.
