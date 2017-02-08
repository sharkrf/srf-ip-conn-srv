This is an optional replacement to the provided dashboard that uses PHP.

This option uses NodeJS rather than PHP to connect to the openSpot API through the UNIX socket file.

To setup the connection change to the nodeServerInterface directory and modify the config file.
The latest version of NodeJS 6.9.2+ will need to be installed on the system to proceed.

Now run the following command to install dependencies:
npm install

then the following command to start the application:
node app

---------------

Now modify the serverConfig.json file to point to the location of the node application that was started.

Now the data is populated on the index page is being pulled through the NodeJS application.

