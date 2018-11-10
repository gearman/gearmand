#brew install boost
wget https://bootstrap.pypa.io/get-pip.py
sudo -H python get-pip.py
sudo -H pip install virtualenv
virtualenv sphinx
. sphinx/bin/activate
pip install sphinx
