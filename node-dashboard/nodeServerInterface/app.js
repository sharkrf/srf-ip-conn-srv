'use strict';

const net = require('net');
const express = require('express');
const config = require('./config.json');
const app = express();

function sendRequest(request) {
  return new Promise((resolve) => {
    const openSpotAPI = net.createConnection({ path: config.apiSocketFile, readable: true, writable: true }, () => {
      openSpotAPI.write(request);
    });
    openSpotAPI.on('data', (data) => {
      resolve(data.toString());
    });
  });
}

app.listen(config.hostPort, () => {
  console.log(`Server started - listening on port ${config.hostPort}`);
});

app.get('/serverDetails', (req, res) => {
  const jsonDetails = '{"req":"server-details"}';
  sendRequest(jsonDetails)
  .then((data) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Content-Type', 'application/json');
    res.send(data);
  });
});

app.get('/clientList', (req, res) => {
  const jsonDetails = '{"req":"client-list"}';
  sendRequest(jsonDetails)
  .then((data) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Content-Type', 'application/json');
    res.send(data);
  });
});

app.get('/showInfo/:id', (req, res) => {
  const jsonDetails = `{"req":"client-config","client-id":"${req.params.id}"}`;
  sendRequest(jsonDetails)
  .then((data) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Content-Type', 'application/json');
    res.send(data);
  });
});

app.get('/lastHeard', (req, res) => {
  const jsonDetails = '{"req":"lastheard-list"}';
  sendRequest(jsonDetails)
  .then((data) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Content-Type', 'application/json');
    res.send(data);
  });
});
