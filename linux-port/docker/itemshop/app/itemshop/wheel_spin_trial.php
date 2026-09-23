<?php
	// "Probne" zakrecenie dla trybu podgladu (?preview=1, iframe na
	// m2singleplayer.pl) - ten sam wazony los z tej samej 16-tki co
	// pokazuje sie na kole (patrz $_SESSION['wheel_active_ids'], ustawiane
	// przez pages/wheel.php i wheel_reroll.php), ale BEZ zadnych realnych
	// skutkow: brak logowania, brak zmiany salda, brak wpisu do
	// player.item_award, brak logu. Czysto pokazowe - "co bys dostal".
	session_start();
	require("../user/config.php");
	header('Content-Type: application/json; charset=utf-8');

	$activeIds = isset($_SESSION['wheel_active_ids']) ? $_SESSION['wheel_active_ids'] : array();
	if (empty($activeIds)) {
		echo json_encode(array('ok' => false, 'error' => 'Odswiez strone Kola Losu i sprobuj ponownie.'));
		exit;
	}
	$idList = implode(',', array_map('intval', $activeIds));

	$prizeRes = mysqli_query($sqlServ, "SELECT id, name_item, item_desc, vnum_icon, count, weight, is_jackpot FROM itemshop.wheel_prizes WHERE id IN ($idList) ORDER BY FIELD(id,$idList)");
	$prizes = array();
	$totalWeight = 0;
	while ($p = mysqli_fetch_assoc($prizeRes)) {
		$prizes[] = $p;
		$totalWeight += (int)$p['weight'];
	}
	if (empty($prizes) || $totalWeight <= 0) {
		echo json_encode(array('ok' => false, 'error' => 'Kolo Losu jest chwilowo niedostepne.'));
		exit;
	}

	$roll = random_int(1, $totalWeight);
	$won = null;
	$wonIndex = 0;
	$acc = 0;
	foreach ($prizes as $idx => $p) {
		$acc += (int)$p['weight'];
		if ($roll <= $acc) {
			$won = $p;
			$wonIndex = $idx;
			break;
		}
	}
	if ($won === null) {
		$won = end($prizes);
		$wonIndex = count($prizes) - 1;
	}

	echo json_encode(array(
		'ok' => true,
		'position' => $wonIndex + 1,
		'name' => $won['name_item'],
		'desc' => $won['item_desc'],
		'count' => (int)$won['count'],
		'icon' => $won['vnum_icon'],
		'jackpot' => (bool)$won['is_jackpot'],
	));
