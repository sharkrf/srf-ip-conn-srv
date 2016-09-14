# SharkRF IP Connector server

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
