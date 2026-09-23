<?php
	// Ten skrot odpowiada twardo zakodowanej sciezce "/ishop?pid=..." budowanej
	// przez ACMD(do_in_game_mall) w cmd_general.cpp (galaz LC_IsEurope):
	//   sas = md5(pid . aid . "<sekret_wspoldzielony_z_serwerem_gry>")
	// Odtwarzamy dokladnie ta sama funkcje po stronie PHP: pid znamy z URL,
	// aid (account_id) doczytujemy z player.player, i porownujemy wyliczony
	// hash z tym co przyslal klient. Zgodny hash = dokladnie ten gracz, ktory
	// wlasnie kliknal monetke w grze - logujemy go bez formularza.
	session_start();
	require("../user/config.php");

	$pid = isset($_GET['pid']) ? (int)$_GET['pid'] : 0;
	$sas = isset($_GET['sas']) ? $_GET['sas'] : '';

	$ok = false;
	$failReason = 'no pid/sas';
	if ($pid > 0 && $sas !== '') {
		$pidEsc = mysqli_real_escape_string($sqlServ, (string)$pid);
		$res = mysqli_query($sqlServ, "SELECT p.account_id, a.login, a.cash, a.mileage "
			. "FROM player.player p INNER JOIN account.account a ON a.id = p.account_id "
			. "WHERE p.id = '$pidEsc'");
		if (!$res) {
			$failReason = 'query error: ' . mysqli_error($sqlServ);
		}
		$row = $res ? mysqli_fetch_assoc($res) : null;
		if (!$row) {
			$failReason = 'no player row for pid=' . $pid;
		} else {
			// UWAGA: ten sam sekret musi byc wpisany w ACMD(do_in_game_mall)
			// (cmd_general.cpp, zmienna sas_key) po stronie rdzenia gry -
			// oryginalna wartosc zamaskowana przed udostepnieniem tej paczki.
			// The r40250 game core signs the link with the literal "GF9001"
			// (cmd_general.cpp, ACMD(do_in_game_mall)); M2_MALL_SAS_KEY is
			// for a core built with another key.
			$expected = md5($pid . $row['account_id'] . (getenv('M2_MALL_SAS_KEY') ?: 'GF9001'));
			if (hash_equals($expected, $sas)) {
				$_SESSION['id'] = $row['login'];
				$_SESSION['acc_id'] = $row['account_id'];
				$ok = true;
			} else {
				$failReason = 'sas mismatch for pid=' . $pid . ' aid=' . $row['account_id'];
			}
		}
	}

	mysqli_close($sqlServ);

	// Diagnostyka dla sporadycznego "Dostep zabroniony" - loguje tylko
	// nieudane proby, zeby zlapac przyczyne przy nastepnym takim przypadku
	// bez zaladowywania logow normalnym ruchem.
	if (!$ok) {
		error_log("ishop auto-login failed: pid=" . $pid . " reason=" . $failReason
			. " ip=" . (isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '?'));
	}

	if ($ok) {
		header("Location: /itemshop/");
	} else {
		// Brak/zly pid+sas - ktos wszedl tu bez przejscia przez monetke w grze.
		header("Location: /itemshop/?s=login");
	}
	exit;
?>
