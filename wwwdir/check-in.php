<?php

// This is basically a working placeholder, you can build elaborate update mechanisms!
//
// The ESP32 will send a association request with the following data: {"type":"ASSOCREQ","src":"00:00:02:7C:4D:45:3B:18","protover":0,"state":{"batteryMv":2600,"hwType":8,"swVer":1172526071808},"screenPixWidth":128,"screenPixHeight":296,"screenMmWidth":29,"screenMmHeight":67,"compressionsSupported":2,"maxWaitMsec":200,"screenType":16,"rfu":""}
// This script should answer with something like this: {"checkinDelay":900000,"retryDelay":1000,"failedCheckinsTillBlank":2,"failedCheckinsTillDissoc":0}
//
// A checkin from the ESP32 looks like this: {"type":"CHECK_IN","src":"00:00:02:7C:4D:45:3B:18","state":{"batteryMv":2550,"hwType":8,"swVer":1172526071808},"lastPacketLQI":57,"lastPacketRSSI":-73,"rfu":"","temperature":152}
// And this is a valid answer: {"imgFile":"img\\/0000027C4D453B18.bmp","imgMTime":1663104505,"imgCTime":1663104505,"imgLen":4798,"imgMd5":"59bdc79463de515fc3bb7048b610c2a6","osLen":0,"osMTime":0,"osCTime":0,"osFile":0,"osMd5":0,"rfu":0}
//
//  Note that you can reply with any filename you like; this allows you to have multiple tags point to the same file. The file will only be cached once in the ESP32

$json = file_get_contents("php://input");
error_log($json);
$arr = json_decode($json, true);

if(!$arr)die();


switch($arr['type']){
	case "ASSOCREQ":
		processAssocReq($arr);
		break;
	case "CHECK_IN":
		processCheckIn($arr);
		break;
}

function processAssocReq($arr){
	$ret = array();
       	$ret['checkinDelay']=900000;
        $ret['retryDelay']=1000;
        $ret['failedCheckinsTillBlank']=2;
        $ret['failedCheckinsTillDissoc']=0;
        $json = json_encode($ret);
        error_log($json);
        echo $json;
}

function processCheckIn($arr){
	$file = "img/".str_replace(":","",$arr['src']).".bmp";
	if(!file_exists($file)){
	exec("cp img/default.bmp ".escapeshellarg($file));
	}
	$ret = array();
	$ret['imgFile']=$file;
	$ret['imgMTime']=filemtime($file);
	$ret['imgCTime']=filectime($file);
	$ret['imgLen']=filesize($file);
	$ret['imgMd5']=md5_file($file);
	$ret['osLen']=0;
	$ret['osMTime']=0;
	$ret['osCTime']=0;
	$ret['osFile']=0;
	$ret['osMd5']=0;
	$ret['rfu']=0;

	$json = json_encode($ret);
	error_log($json);
	echo $json;
}


