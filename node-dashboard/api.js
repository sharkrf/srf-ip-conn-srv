var nodeServerIP = "127.0.0.1"; // IP Address/hostname that is accessible through internet
var nodeServerPort ="8081"; //port the nodeServerInterface is hosted on 8081 by defaul

var srf_ajax_last_retry_at = new Date();
var srf_timer_uptime = null;
var srf_server_uptime;
var srf_callsigns = [];
var nodeConnectionURL = 'http://' + nodeServerIP + ':' + nodeServerPort;

function srf_ajax(url, success, error, always) {
	return $.ajax({
		type: 'GET',
		url: url,
		timeout: 3000,
		tryCount: 0,
		retryLimit: 15,
		error: function(xhr, textStatus, errorThrown) {
			this.tryCount++;
			if (this.tryCount <= this.retryLimit) {
				// Try again.
				var d = new Date();
				if (d.getTime()-srf_ajax_last_retry_at.getTime() < 500) {
					var query = this;
					setTimeout(function() {
						srf_ajax_last_retry_at = new Date();
						$.ajax(query).success(success).error(error).always(always);
					}, 1000+Math.random()*3000);
				} else {
					$.ajax(this).success(success).error(error).always(always);
					srf_ajax_last_retry_at = new Date();
				}
			}
			return;
		}
	}).success(success).error(error).always(always);
}

function srf_req(url, success, error, always) {
	return srf_ajax(url, function(response) {
		if (response.errno) {
			alertify.error('Error during request (' + response.errno + '): ' + response.errstr);
			if (error)
				error();
		} else {
			if (success)
				success(response);
		}
	}, function(response, textstatus, httperror) {
		alertify.error('Error during request: ' + httperror);
		if (error)
			error(response, textstatus, httperror);
	}, always);
}

function srf_pad_time(s) {
	return (s < 10 ? '0' : '') + s;
}

function srf_update_server_uptime() {
	var days = Math.floor(srf_server_uptime / 86400);
	var uptime_currday = srf_server_uptime-days*86400;
	var hours = Math.floor(uptime_currday / 3600);
	var minutes = Math.floor((uptime_currday % 3600) / 60);
	var seconds = Math.floor(uptime_currday % 60);

	$('#server-uptime').html((days > 0 ? (days + ' day' + (days > 1 ? 's' : '') + ', ') : '') + srf_pad_time(hours) + ':' + srf_pad_time(minutes) + ':' + srf_pad_time(seconds));

	srf_server_uptime++;
}

function srf_server_info_update() {
	srf_req(nodeConnectionURL + '/serverdetails', function(response) {
		$('#server-name').html(response.name);
		$('#server-desc').html(response.desc);
		$('#server-contact').html(response.contact);
		srf_server_uptime = response.uptime;
		srf_update_server_uptime();
		if (srf_timer_uptime)
			clearInterval(srf_timer_uptime);
		srf_timer_uptime = setInterval(srf_update_server_uptime, 1000);
		$('#server-githash').html(response.githash);

		var protos = '';
		if (response['allow-raw'])
			protos += 'raw';

		if (response['allow-dmr']) {
			if (protos != '')
				protos += ', ';
			protos += 'DMR';
		}

		if (response['allow-dstar']) {
			if (protos != '')
				protos += ', ';
			protos += 'D-STAR';
		}

		if (response['allow-c4fm']) {
			if (protos != '')
				protos += ', ';
			protos += 'C4FM';
		}

		if (response['allow-nxdn']) {
			if (protos != '')
				protos += ', ';
			protos += 'NXDN';
		}

		if (protos == '')
			protos = 'None';

		$('#server-allowed-protos').html(protos);
	});
}

function srf_connected_clients_update() {
	srf_req(nodeConnectionURL + '/clientList', function(response) {
		$('#client-list tbody tr').remove();
		$.each(response.list, function(key, value) {
			var last_pkt_at;
			if (value['last-pkt-at'] == 0)
				last_pkt_at = 'N/A';
			else {
				last_pkt_at = new Date(value['last-pkt-at'] * 1000).toLocaleString('en-GB', {
					day: 'numeric',
					month: 'short',
					year: 'numeric',
					hour: 'numeric',
					minute: 'numeric',
					second: 'numeric'
				});
			}
			var gotconfig;
			if (value['got-config'] == 0)
				gotconfig = 'N/A';
			else {
				gotconfig = '<button type="button" class="btn btn-sm btn-info" ' +
					'onclick="javascript:srf_showinfo(' + value.id + ')">Info</button>';
				srf_callsigns[value.id] = value.callsign;
			}

			$('#client-list tbody').append('<tr>' +
				'<td>' + value.id + (value['got-config'] ? ' (' + value.callsign + ')' : '') + '</td>' +
				'<td>' + last_pkt_at + '</td>' +
				'<td>' + gotconfig + '</td>' +
				'</tr>');
		});
	});
}

function srf_showinfo(id) {
	srf_req(nodeConnectionURL + '/showInfo' + id, function(response) {
		$('#client-info-id').html(response['id']);
		$('#client-info-operator-callsign').html(response['operator-callsign']);
		$('#client-info-hw-manufacturer').html(response['hw-manufacturer']);
		$('#client-info-hw-model').html(response['hw-model']);
		$('#client-info-hw-version').html(response['hw-version']);
		$('#client-info-sw-version').html(response['sw-version']);
		$('#client-info-rx-frequency').html(response['rx-freq']);
		$('#client-info-tx-frequency').html(response['tx-freq']);
		$('#client-info-tx-power').html(response['tx-power']);
		$('#client-info-latitude').html(response['latitude']);
		$('#client-info-longitude').html(response['longitude']);
		$('#client-info-height').html(response['height-agl']);
		$('#client-info-location').html(response['location']);
		$('#client-info-description').html(response['description']);
		$('#client-info').modal('show');
	});
}

function srf_lastheard_update() {
	srf_req(nodeConnectionURL + '/lastHeard', function(response) {
		var i = 0;
		var in_call = response['in-call'];
		$('#last-heard tbody tr').remove();
		$.each(response.list, function(key, value) {
			var at = new Date(value['at'] * 1000).toLocaleString('en-GB', {
					day: 'numeric',
					month: 'short',
					year: 'numeric',
					hour: 'numeric',
					minute: 'numeric',
					second: 'numeric'
				});
			var mode;
			switch (value['mode']) {
				case 0: mode = 'Raw'; break;
				case 1: mode = 'DMR'; break;
				case 2: mode = 'D-STAR'; break;
				case 3: mode = 'C4FM'; break;
				case 4: mode = 'NXDN'; break;
				default: mode = 'Unknown'; break;
			}
			var duration;
			var minutes = Math.floor(value['duration']/60);
			var seconds = value['duration']-minutes*60;
			if (minutes)
				duration = minutes + ' min ' + seconds + ' sec';
			else
				duration = seconds + ' sec';

			if (i == 0 && in_call)
				duration += ' <span class="in-call">(in call)</span>';

			$('#last-heard tbody').append('<tr>' +
				'<td>' + value.to + '</td>' +
				'<td>' + value.from + '</td>' +
				'<td>' + (value['is-group-call'] ? 'Group' : 'Private') + '</td>' +
				'<td>' + value['client-id'] + (srf_callsigns[value['client-id']] != undefined ? ' (' + srf_callsigns[value['client-id']] + ')' : '') + '</td>' +
				'<td>' + at + '</td>' +
				'<td>' + mode + '</td>' +
				'<td>' + duration + '</td>' +
				'</tr>');
			i++;
		});
	});
}

$(function() {
	srf_server_info_update();
	srf_connected_clients_update();
	setInterval(srf_connected_clients_update, 1000);
	srf_lastheard_update();
	setInterval(srf_lastheard_update, 1000);
});
