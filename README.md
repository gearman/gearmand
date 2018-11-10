gearmand
========

[![Build Status](https://travis-ci.org/gearman/gearmand.png)](https://travis-ci.org/gearman/gearmand)

The latest version of ```gearmand``` source code and versions 1.1.13 and later can be found at [GitHub Repository](https://github.com/gearman/gearmand). Older versions released before 1.1.13 can be found at [Launchpad Repository](http://launchpad.net/gearmand/).

You can grab the latest release distribution of Gearman from https://github.com/gearman/gearmand/releases


What Is Gearman?
----------------

Gearman provides a generic application framework to farm out work to other machines or processes that are better suited to do the work. It allows you to do work in parallel, to load balance processing, and to call functions between languages. Gearman is the nervous system for how distributed processing communicates.

If you downloaded this package as a ```tar.gz``` distribution you'll want to read ***Getting Started*** section below or visit the more detailed web page [Getting Started](http://gearman.org/getting-started/)

If you are interested in developing or submitting patches to the project, read the ***Contributing*** section below and check out the [HACKING](https://github.com/gearman/gearmand/blob/master/HACKING) file for ***Coding Style*** and [COPYING](https://github.com/gearman/gearmand/blob/master/COPYING) for details on ***licensing***.


Getting Started
---------------

If you want to work on the latest code, please read the file [HACKING](https://github.com/gearman/gearmand/blob/master/HACKING).

To build a release version from a tarball (```.tar.gz``` or ```.tgz```), you can follow the normal:

Change into the directory where you saved the tarball and run:

    tar xzf gearmand-X.Y.tar.gz
    cd gearmand-X.Y
    
Then run the usual autoconfigure style build (you may need to use ```sudo``` to install):

    ./configure
    make
    make install

You can also run ```make test``` before installing to make sure everything
checks out ok. You can also streamline the process of building and testing by running:

    ./configure && make && make test
    make install

Once you have it installed, you can start the Gearman job server with:

    gearmand -v

This will start it while printing some verbose messages. To try
running a job through it, look in the examples/ directory of this
source and run:

    ./reverse_worker

Once that is running, you can run your first job with:

    ./reverse_client "Hello, Gearman!"

If all goes well, the reverse_worker application should have output:

    Job=H:lap:1 Workload=Hello, Gearman! Result=!namraeG ,olleH

While the reverse_client returned:

    Result=!namraeG ,olleH
    
There are a lot more details about gearmand at [Getting Started](http://gearman.org/getting-started/).

If you want to start writing your own client and workers, be sure to check out the [Developer API](http://gearman.info/libgearman.html) documentation.

There are also many other [Useful Resources](http://www.gearman.org/) to help you put gearmand to work for you!

Enjoy!


Contributing
------------

The current versions of geamand are maintained on our [GitHub Repo for gearmand](https://github.com/gearman/gearmand).

If you are not familiar with ```git```, you can find more info at [Getting Started with Git](https://git-scm.com/book/en/v1/Getting-Started).

Please follow these instructions to clone, create a branch, and generate a pull request on that branch. More details on using GitHub can be found at [GitHub Help](https://help.github.com/).

1. Clone the GitHub repository to your local file system:

        git clone https://github.com/gearman/gearmand

   If you do not have access to create branches in the gearmand GitHub repository, you will probably want to fork the repository and clone your fork instead. Refer to [Contributing to Open Source on GitHub](https://guides.github.com/activities/contributing-to-open-source/#contributing) for details.

2. Next, think of a clear, descriptive branch name and then create a new branch and change to it:

        cd gearmand
        git checkout -b DESCRIPTIVE_BRANCH_NAME

3. Once the tree is branched you will need to generate the "configure" script for autoconfigure.

        ./bootstrap.sh -a

4. Finally, you are ready to run tests, make changes to the code, commit and push them to GitHub, and generate a pull request on your branch so we can consider your changes.

You can learn more about how to [Create a Pull Request](https://help.github.com/articles/creating-a-pull-request/) and [Create a Pull Request from a Fork](https://help.github.com/articles/creating-a-pull-request-from-a-fork/).


But Wait! There's More!
-----------------------

Once you have made your changes there are two additional ```make``` targets to build release ready distributions:

To generate a tarball distribution of your code:

    make dist
    
Or to generate an RPM distribution use:

    make rpm
    
Thanks and keep hacking!

Cheers,  
  -Brian  
  Seattle, WA.
