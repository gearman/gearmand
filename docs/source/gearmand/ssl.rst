======================
Using SSL with Gearman
======================

If you are not paying a certificate authority to generate a certificate for you, you will first need a certificate authority (CA) for gearmand::

   openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:4096 -out gearmand-ca.key

   openssl req -x509 -new -key gearmand-ca.key -sha256 -days 3650 -subj "/CN=Internal Gearman Root CA" -out gearmand-ca.pem

You then need to place your CA certificate into the directory from which the server will read it.

Generate a server certificate for the server to use. You will need to create an extension file named "gearmand_server_ext.cnf" in order to support Subject Alternative Names (SAN), which are required by modern TLS clients. It should look something like the following (add and/or replace the hostnames and IP addresses under "[alt_names]" as needed)::

   authorityKeyIdentifier=keyid,issuer
   basicConstraints=CA:FALSE
   keyUsage = digitalSignature, keyEncipherment
   extendedKeyUsage = serverAuth, clientAuth
   subjectAltName = @alt_names

   [alt_names]
   DNS.1 = gearmand-server.local
   DNS.2 = localhost
   IP.1 = 127.0.0.1
   # Add your internal server IP if clients connect directly via IP
   # IP.2 = 10.x.x.x

Then, generate the key, the request, and the signed certificate (valid for 2 years or 730 days)::

   openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out gearmand.key

   openssl req -new -key gearmand.key -subj "/CN=gearmand-server.local" -out gearmand.req

   openssl x509 -req -in gearmand.req -CA gearmand-ca.pem -CAkey gearmand-ca.key -CAcreateserial -out gearmand.pem -days 730 -sha256 -extfile gearmand_server_ext.cnf

Note that the :option:`-CAcreateserial` argument in that last command will cause a :file:`gearmand-ca.srl` file to be automatically created in the current working directory the first time you execute it.

Finally, generate a certificate for clients and workers to use::

   openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -out gearman.key

   openssl req -new -key gearman.key -subj "/CN=gearman-client" -out gearman.req

   openssl x509 -req -in gearman.req -CA gearmand-ca.pem -CAkey gearmand-ca.key -CAcreateserial -out gearman.pem -days 730 -sha256

Specifying Subject Alternative Names (SAN) is not needed for clients/workers since they are not hostname-verified.

Caveats:

  1. Make sure your gearmand was configured with '--enable-ssl'. You will likely need to compile from source as packagers tend to not enable that option.

  2. The commands above generate server and client certificates valid for 730 days (2 years). If you want a different lifetime, adjust the '-days' parameters in the commands accordingly. The lifetimes of the server and client certificates should be less than or equal to the number of days until the CA certificate expires (3650 in the commands above).

  3. Make sure you specify different "Common Name" ("/CN=...") values when generating the certificates for the CA, server, and client. OpenSSL does not like them being the same.
