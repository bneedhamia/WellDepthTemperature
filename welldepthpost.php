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

const NUM_SENSORS = 12; // expected number of temperature sensors.

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
// Outputs the result of our page.
// boolean success: true if we were successful, false otherwise.
// string reason: if non-null, an additional explanitory text.
function sendresult($succeeded, $reason) {
    //TODO change to respond in JSON, since it's easier to parse.

    // build the XML response, quoting the reason field properly.
    $xml = '<?xml version="1.0" encoding="utf-8"?>';
    $xml .= '<response>';

    $xml .= '<succeeded>';
    if ($succeeded) {
        $xml .= 'true';
    } else {
        $xml .= 'false';
    }
    $xml .= '</succeeded>';

    if (!is_null($reason)) {
        $xml .= '<reason>';
        $xml .= htmlspecialchars($reason, ENT_QUOTES);  // Quote all XML/HTML special characters.
        $xml .= '</reason>';
    }

    $xml .= '</response>';

    // Write out the XML response, setting the HTML headers properly
    // NOTE: The page must not have produced any output prior to these header() calls.
    header('Content-type: application/xml');
    header('Content-length: ' . strlen($xml));
    echo $xml;

    // That's all we do.
    if (!$success) {
        exit(1);
    }
    exit;
}

/////////////////////////////////// Main Program ///////////////////////////

// Extract the configuration parameters.

$config = parse_ini_file("welldepthconfig.php");

$app_username = $config['app_username'];
$app_password = $config['app_password'];

$database = $config['sql_database'];
$sql_username = $config['sql_writer_username'];
$sql_password = $config['sql_writer_password'];

// Extract the HTTPS POST parameters

$post_user = $_POST['username'];
$post_pass = $_POST['password'];
if (strcmp($post_user, $app_username) != 0) {
    sendResult(FALSE, 'invalid username, password');
}
if (strcmp($post_pass, $app_password) != 0) {
    sendResult(FALSE, 'invalid username, password');
}

$wellDepthM = toFloat($_POST['well_depth_m']);

$wellTempC = array();
for ($index = 0; $index < 12; ++$index) { //TODO constant
    $wellTempC[$index] = toFloat($_POST['well_temp_c' . $index]);
}

//echo 'welldepthm=' . $wellDepthM . '<br>';
//for ($index = 0; $index < 12; ++$index) { //TODO constant
//    echo 'temp[' . $index . ']=' . $wellTempC[$index] . '<br>';
//}

//TODO perform the sql insertion.

// Respond that the Post succeeded.
sendresult(TRUE, NULL);

?>
