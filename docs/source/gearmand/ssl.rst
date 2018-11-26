======================
Using SSL with Gearman
======================

If you are not paying a certificate authority to generate a certificate for you, you will first need a certificate authority (CA) for gearmand:

   openssl req -config /etc/pki/tls/openssl.cnf -new -x509 -keyout gearmand-ca.key -out gearmand-ca.pem -days 3650 

   echo "00" > gearmand.srl

You then need to place your CA certificate into the directory from which the server will read it.

Generate a server certificate for the server to use:

   openssl genrsa -out gearmand.key 1024 

   openssl req -key gearmand.key -new -out gearmand.req

   openssl x509 -req -in gearmand.req -CA gearmand-ca.pem -CAkey gearmand-ca.key -CAserial gearmand.srl -out gearmand.pem 

Generate a client certificate for client/workers to use:

  openssl genrsa -out gearman.key 1024 

  openssl req -key gearman.key -new -out gearman.req 

  openssl x509 -req -in gearman.req -CA gearmand-ca.pem -CAkey gearmand-ca.key -CAserial gearmand.srl  -out gearman.pem


Caveats:

  1. Make sure your gearmand was configured with '--enable-ssl'. You will likely need to compile from source.

  2. Unless you want your server and client certificates to expire in 30 days (the default), you should add the arguments '-days X' to the 'open req' commands for generating those certificates, where X is the number of days after which your certificates will expire. It should be less than or equal to the number of days until the CA certificate expires (3650 in the commands above).

  3. Make sure you specify different "Common Name" values when generating the certificates for the CA, server, and client. OpenSSL does not like them being the same.
