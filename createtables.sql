CREATE TABLE depth (
  record_id INTEGER NOT NULL AUTO_INCREMENT,	# unique ID of each record, managed by MySQL.
  server_time_secs INT(4) NOT NULL, # server time (seconds since the Epoch) the HTTPS POST was received.
  well_depth_m FLOAT NULL,	# estimated depth of the well, in meters. NULL means unable to estimate.
  well_temp_c0 FLOAT NULL,	# temperature (degrees C) of the lowest sensor. NULL means sensor is offline.
  well_temp_c1 FLOAT NULL,	# temperature of the sensor above well_temp_c0.
  well_temp_c2 FLOAT NULL,
  well_temp_c3 FLOAT NULL,
  well_temp_c4 FLOAT NULL,
  well_temp_c5 FLOAT NULL,
  well_temp_c6 FLOAT NULL,
  well_temp_c7 FLOAT NULL,
  well_temp_c8 FLOAT NULL,
  well_temp_c9 FLOAT NULL,
  well_temp_c10 FLOAT NULL,
  well_temp_c11 FLOAT NULL,
  PRIMARY KEY (record_id)
);
