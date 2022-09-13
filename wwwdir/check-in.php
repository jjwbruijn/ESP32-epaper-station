<?php
$json = file_get_contents("php://input");
error_log($json);
$arr = json_decode($json, true);


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