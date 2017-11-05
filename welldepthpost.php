<?php
// welldepthpost.php = a web service that enters well depth and
// temperature data into a database. Intended to be called from
// an ESP8266 running WellDepthTemperature.ino.
// Note: this web service is intended to be called only via HTTPS,
// not HTTP.
//
// Copyright (c) 2017 Bradford Needham.
// Licensed under GNU General Public License Version 2.0,
// a copy of which should have been distributed with this program.

const NUM_SENSORS = 12; // maximum number of temperature sensors.

//////////
// Returns the floating-point value of the given string,
//   or NULL if the string is NULL,
//   or calls sendResult() for failure.
//
// $s = the string to convert, or NULL.
function toFloat($s) {
    if ($s == NULL) {
        return NULL;
    }
    
    $result = filter_var($s, FILTER_VALIDATE_FLOAT);
    if ($result == FALSE) {
        sendResult(FALSE, 'Not a floating-point number: ' . $s);
    }
    return $result;
}

//////////
// Outputs the result of our page, in JSON format.
// boolean success: 1 if we were successful, 0 otherwise.
// string reason: if non-null, an additional explanitory text.
function sendresult($succeeded, $reason) { 
    $json = '{';
    
    $json .= '"succeeded": ';
    if ($succeeded) {
      $json .= '1';
    } else {
      $json .= '0';
    }
    
    if (!is_null($reason)) {
        $json .= ', "reason": ';
        // Should escape only ", but htmlspecialchars does that and more.
        $json .= '"' . htmlspecialchars($reason, ENT_QUOTES) . '"';
    }
    
    $json .= '}';

    // Write out the response, setting the HTML headers properly
    // NOTE: The page must not have produced any output prior to these header() calls.
    header('Content-type: application/json');
    header('Content-length: ' . strlen($json));
    echo $json;

    // That's all we do.
    if (!$success) {
        exit(1);
    }
    exit;
}

/////////////////////////////////// Main Program ///////////////////////////

// Extract the configuration parameters.
// NOTE: we do not validate the fields, because we assume they're
// as safe from malicious modification as this .php script.

$config = parse_ini_file("welldepthconfig.php");

$app_username = $config['app_username'];
$app_password = $config['app_password'];

$sql_hostname = $config['sql_hostname'];
$sql_database = $config['sql_database'];
$sql_username = $config['sql_writer_username'];
$sql_password = $config['sql_writer_password'];

// Get the time to put into the SQL record
$now = time();

// Test whether the username and password match. If not, stop.

$post_user = $_POST['username'];
$post_pass = $_POST['password'];
if (strcmp($post_user, $app_username) != 0) {
    sendResult(FALSE, 'invalid username, password');
}
if (strcmp($post_pass, $app_password) != 0) {
    sendResult(FALSE, 'invalid username, password');
}

// Extract the depth and temperatures,
// failing if any is not a float or NULL.
// NOTE: extracting the floats should defeat any SQL injection attack.

$wellDepthM = toFloat($_POST['well_depth_m']);

$wellTempC = array();
for ($index = 0; $index < NUM_SENSORS; ++$index) { //TODO constant
    $wellTempC[$index] = toFloat($_POST['well_temp_c' . $index]);
}

// Build the SQL INSERT statement:
// $fields = a string of the names of the fields to insert
// $values = a string of the values of the fields, in $fields order.

$fields = 'server_time_secs';
$values = "$now";
if (!is_null($wellDepthM)) {
    $fields .= ', well_depth_m';
    $values .= ", $wellDepthM";
}
for ($index = 0; $index < NUM_SENSORS; ++$index) {
    if (!is_null($wellTempC[$index])) {
        $fields .= ', well_temp_c' . $index;
        $values .= ", $wellTempC[$index]";
    }
}
$query = "INSERT into depth ($fields) VALUES ($values)";

// Connect to the database

$link = mysqli_connect($sql_hostname, $sql_username, $sql_password);
if (!$link) {
    $errno = mysqli_errno();
    $err = mysqli_connect_error();
    sendresult(FALSE, 'Failed to connect to database: ' . $errno . ', ' . $err);
}
$db_selected = mysqli_select_db($link, $sql_database);
if (!$db_selected) {
    $errno = mysqli_errno();
    $err = mysqli_error($link);
    mysqli_close($link);
    sendresult(FALSE, 'Failed to select database: ' . $errno . ', ' . $err);
}

// Insert the record.

if (!mysqli_query($link, $query)) {
    $errno = mysqli_errno();
    $err = mysqli_error($link);
    mysqli_close($link);
    sendresult(FALSE, 'Failed to insert into database: ' . $errno . ', ' . $err . '. Query: ' . $query);
}

// We're finished with the SQL connection.
mysqli_close($link);

// Respond that the Post succeeded.
sendresult(TRUE, NULL);

?>
