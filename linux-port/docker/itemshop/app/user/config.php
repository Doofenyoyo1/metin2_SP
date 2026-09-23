<?php
	// From the container's environment (docker-compose.yml passes the same
	// M2_DB_* the game and the panels use), never from this file: the copy
	// this came from had the password written into it.
	DEFINE('SQL_HOST', getenv('M2_DB_HOST') ?: 'mariadb');
	DEFINE('SQL_USER', getenv('M2_DB_USER') ?: 'metin2');
	DEFINE('SQL_PASS', getenv('M2_DB_PASSWORD') ?: '');

	$sqlServ = mysqli_connect(SQL_HOST, SQL_USER, SQL_PASS);

	if (mysqli_connect_errno()) {
		echo "Failed to connect to MySQL: " . mysqli_connect_error();
		exit();
	}
	mysqli_set_charset($sqlServ, "utf8mb4");
?>
