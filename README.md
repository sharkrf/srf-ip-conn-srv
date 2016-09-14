# SharkRF IP Connector Protocol server

This daemon can be used to create your own private radio network. You can
connect to this server using the
[SharkRF IP Connector Protocol](https://github.com/sharkrf/srf-ip-conn).
All connected and logged in clients receive the data you send to the server.

Currently only [openSPOT](sharkrf.com/openspot) supports this protocol.

Pull requests are welcome!

## Compiling
### Prerequisites
You'll need cmake on your system. Install it with *apt-get install cmake*
or *yum install cmake*.

Clone this repo by issuing the command:

```
git clone https://github.com/sharkrf/srf-ip-conn-srv
```

You also need to clone [srf-ip-conn](https://github.com/sharkrf/srf-ip-conn)
and [jsmn](https://github.com/zserge/jsmn):

```
git clone https://github.com/sharkrf/srf-ip-conn
git clone https://github.com/zserge/jsmn
```

### Compiling
```
cd srf-ip-conn-srv/build
SRF_IP_CONN_PATH=/path/to/srf-ip-conn JSMN_PATH=/path/to/jsmn ./build-release.sh
```

The binary will be in *srf-ip-conn-srv/build/Release*.

## Running
By default, srf-ip-conn-srv forks to the background and logs to the syslog
(/var/log/syslog on Debian-based systems). You can force foreground mode by
using the *-f* command line argument. It looks for the configuration file
*config.json* in the current directory, but you can override the config file's
path with the *-c* switch.

### Configuration
There's an example config file *config-example.json* in this repository.
Available configuration options:

- **port**: the UDP port where the daemon listens. It's 65100 by default.
- **ipv4-only**: you can force IPv4-only operation if you set this to 1.
  It's 1 by default.
- **bind-ip**: you can force binding to the specified IP address. By default,
  the daemon listens on all interfaces
- **max-clients**: the number of max. clients logged in to the server.
  The default value is 1000.
- **client-login-timeout-sec**: if the client gets idle during login, it will
  be deleted from the login clients list after this many seconds. The default
  value is 10.
- **client-timeout-sec**: if the client is idle (not sending keepalives or any
  data) for this many seconds, it will be deleted from the clients list.
  The default value is 30.
- **server-password**: the server password. The default value is empty.
- **auth-fail-ip-ignore-sec**: if a client tries to log in with an invalid
  password, it's IP address will be ignored for this many seconds to prevent
  brute force password attacks. The default value is 5.
- **pidfile**: the daemon puts it's PID into this file, and deletes the file
  when it's closed.
- **api-socket-file**: this is the Unix socket file which can be used to
  access the server's API. The default value is "/tmp/srf-ip-conn-srv.socket".
- **server-name**: name of the server, this is displayed on the dashboard.
  The default value is "SharkRF IP Connector Protocol Server".
- **server-description**: description of the server, this is displayed on the
  dashboard. The default value is empty.
- **server-contact**: contact info for the server, this is displayed on the
  dashboard. The default value is empty.

## Dashboard
The server has a web-based dashboard. In order to use it, you need to serve it
with your web server.

The web interface periodically queries data from the PHP scripts, which
interact with the running srf-ip-conn-srv process with JSON request and replies
through Unix domain sockets.

Each request and response has a **req** field which describes the type of the
request and the response.

Valid **req** values:

- **client-list**: requests the client list. The response looks like this:

```
{
	"req": "client-list",
	"list":
	[
		{"id":2161005,"last-data-pkt-at":0},
		{"id":2161006,"last-data-pkt-at":0}
	]
}
```

  Each client has it's own entry in the "list" array. The "last-data-pkt-at"
  field has the value of the last valid data packet (raw, DMR, D-STAR, C4FM)
  received in Unix timestamp. It's 0 if no data packet has been received from
  the client yet.

- **server-details**: requests server info. The response looks like this:

```
{
	"req": "server-details",
	"name": "SharkRF IP Connector Protocol Server",
	"desc": "No description",
	"contact": "ha2non@sharkrf.com",
	"uptime": 1635,
	"githash": "d04633f145069ed4d60d17bf76df447f459f1ed4"
}
```

  "name", "desc" and "contact" fields have the values set in srf-ip-conn-srv's
  config file. "uptime" is in seconds. The "githash" field is the compiled
  version's Git hash.

- **client-config**: requests client config. "client-id" field needs to be
  specified amongst "req" field in the request JSON. The response looks like
  this:

```
{
	"req": "client-config",
	"id": 2161006,
	"got-config": 1,
	"operator-callsign": "HG1MA",
	"hw-manufacturer": "SharkRF",
	"hw-model": "openSPOT",
	"hw-version": "1.0",
	"sw-version": "0054",
	"rx-freq": 435000000,
	"tx-freq": 435000000,
	"tx-power": 13,
	"latitude": "0.000000",
	"longitude": "0.000000",
	"height-agl": "0",
	"location": "",
	"description": "openspot-devicename"
}
```

  "tx-power" is in dBm. If the requested client ID is not found, or the
  server has no config info from the client, "got-config" will be 0 and only
  "req", "id", and "got-config" fields will be included in the response.
