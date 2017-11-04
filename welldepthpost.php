<?php

/////////////////////////////////// Program Begins ///////////////////////////

// Extract the configuration parameters.

$config = parse_ini_file("welldepthconfig.php");

$app_username = $config['app_username'];
$app_password = $config['app_password'];

$database = $config['sql_database'];
$sql_username = $config['sql_writer_username'];
$sql_password = $config['sql_writer_password'];

echo 'app_username=' . $app_username . ', app_password=' . $app_password . '<br>';

echo 'db=' . $database . ', dbuser=' . $sql_username . ', dbpassword=' . $sql_password . '<br>';
echo 'Hello ' . htmlspecialchars($_POST["username"]) . ', ' . htmlspecialchars($_POST["password"]);
?>
