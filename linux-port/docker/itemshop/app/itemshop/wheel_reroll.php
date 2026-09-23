<?php
	// Standalone endpoint (siostrzany do wheel_spin.php - patrz komentarz tam)
	// - losuje nowa 16-tke (14 zwyklych + 2 glowne, glowne zawsze na pozycji
	// 1 i 9) tak samo jak pages/wheel.php przy pelnym wejsciu na strone, ale
	// bez przeladowania calej strony - wywolywane po zamknieciu okienka z
	// wygrana, zeby tylko ikony na kole sie podmienily.
	session_start();
	require("../user/config.php");
	header('Content-Type: application/json; charset=utf-8');

	// Nie wymaga logowania celowo - to samo wywoluje sie w trybie podgladu
	// (?preview=1, iframe na m2singleplayer.pl), gdzie nikt nie jest
	// zalogowany. Nic tu nie kosztuje ani nie dotyka konta, wiec anonimowy
	// dostep jest bezpieczny.
	$mainRes = mysqli_query($sqlServ, "SELECT id FROM itemshop.wheel_prizes WHERE level=1 AND is_jackpot=1 ORDER BY RAND() LIMIT 2");
	$mainIds = array();
	while ($r = mysqli_fetch_assoc($mainRes)) { $mainIds[] = (int)$r['id']; }

	$regRes = mysqli_query($sqlServ, "SELECT id FROM itemshop.wheel_prizes WHERE level=1 AND is_jackpot=0 ORDER BY RAND() LIMIT 14");
	$regIds = array();
	while ($r = mysqli_fetch_assoc($regRes)) { $regIds[] = (int)$r['id']; }

	if (count($mainIds) < 2 || count($regIds) < 14) {
		echo json_encode(array('ok' => false, 'error' => 'Kolo Losu jest chwilowo niedostepne.'));
		exit;
	}

	$byPosition = array();
	$byPosition[1] = $mainIds[0];
	$byPosition[9] = $mainIds[1];
	$regSlot = 0;
	for ($pos = 1; $pos <= 16; $pos++) {
		if ($pos == 1 || $pos == 9) continue;
		$byPosition[$pos] = $regIds[$regSlot];
		$regSlot++;
	}
	ksort($byPosition);
	$activeIds = array_values($byPosition);
	$_SESSION['wheel_active_ids'] = $activeIds;

	$idList = implode(',', $activeIds);
	$prizeRes = mysqli_query($sqlServ, "SELECT id, name_item, vnum_icon FROM itemshop.wheel_prizes WHERE id IN ($idList) ORDER BY FIELD(id,$idList)");
	$items = array();
	while ($p = mysqli_fetch_assoc($prizeRes)) {
		$items[] = array(
			'icon' => $p['vnum_icon'],
			'name' => $p['name_item'],
		);
	}

	echo json_encode(array('ok' => true, 'items' => $items));
