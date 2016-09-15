<?php
	include('config.inc.php');

	header("Content-Type: application/json");

	if (!isset($_GET['req']))
		return;

	$reqname = $_GET['req'];

	$fp = stream_socket_client("unix://$config_api_socket_file", $errno, $errstr, 3);
	if (!$fp) {
		$res = array();
		$res['errno'] = $errno;
		$res['errstr'] = "Can't open Unix socket file to server: $errstr";
		echo json_encode($res);
	} else {
		$req = array();
		$req['req'] = $reqname;
		if (isset($_GET['client-id']))
			$req['client-id'] = $_GET['client-id'];
		fwrite($fp, json_encode($req));
		while (!feof($fp)) {
			$res = fgets($fp, 1024);
			echo $res;
		}
		fclose($fp);
	}
?>