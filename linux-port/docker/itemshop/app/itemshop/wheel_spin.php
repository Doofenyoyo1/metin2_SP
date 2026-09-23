<?php
	// Standalone endpoint (NOT routed through index.php's ?s= page system) -
	// index.php always wraps every page in the full <html><head>...<body> shell
	// before including it, which corrupts a JSON response. This file is its own
	// entry point so the response body is pure JSON, nothing else.
	//
	// Jedno zakrecenie: 10 SM (cash) -> losowanie wazone z itemshop.wheel_prizes
	// (level=1) -> ta sama sciezka dostawy co zwykly zakup (player.item_award)
	// + log do wheel_spin_log.
	session_start();
	require("../user/config.php");
	header('Content-Type: application/json; charset=utf-8');

	const WHEEL_SPIN_COST = 10;

	if (!isset($_SESSION['id']) || !isset($_SESSION['acc_id'])) {
		echo json_encode(array('ok' => false, 'error' => 'Musisz byc zalogowany.'));
		exit;
	}

	$coninfo = mysqli_query($sqlServ, "SELECT cash FROM account.account WHERE id='".(int)$_SESSION['acc_id']."'");
	$info = mysqli_fetch_assoc($coninfo);
	$balance = (int)$info['cash'];
	if ($balance < WHEEL_SPIN_COST) {
		echo json_encode(array('ok' => false, 'error' => 'Za malo Smoczych Monet (SM). Masz '.$balance.', potrzeba '.WHEEL_SPIN_COST.'.'));
		exit;
	}

	// Losuj tylko z tych 16 ID, ktore wheel.php wylosowal i zapisal w sesji
	// przy ostatnim wejsciu na strone kola - to fizycznie to samo 16, ktore
	// gracz widzi teraz na ekranie, nie cala 30-elementowa pula.
	$activeIds = isset($_SESSION['wheel_active_ids']) ? $_SESSION['wheel_active_ids'] : array();
	if (empty($activeIds)) {
		echo json_encode(array('ok' => false, 'error' => 'Odswiez strone Kola Losu i sprobuj ponownie.'));
		exit;
	}
	$idList = implode(',', array_map('intval', $activeIds));

	// Wazone losowanie: suma wag, punkt 1..suma, pierwszy wiersz ktory go
	// przekracza wygrywa. Prosty i wystarczajacy dla garstki wierszy.
	$prizeRes = mysqli_query($sqlServ, "SELECT id, name_item, item_desc, vnum, vnum_icon, count, weight, is_jackpot, socket0, socket1, socket2 FROM itemshop.wheel_prizes WHERE id IN ($idList) ORDER BY FIELD(id,$idList)");
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
			$wonIndex = $idx; // 0-based, id kolejne 1..8 wg ORDER BY id
			break;
		}
	}
	if ($won === null) {
		$won = end($prizes);
		$wonIndex = count($prizes) - 1;
	}

	// Kazda nagroda ma teraz dokladnie jedno pole na kole (patrz pages/wheel.php)
	// - pozycja to wprost numer wygranego wiersza, zero zgadywania.
	$position = $wonIndex + 1;

	$data = date('Y-m-d H:i:s');
	mysqli_query($sqlServ, "UPDATE account.account SET `cash` = `cash` - ".WHEEL_SPIN_COST." WHERE `id` = '".(int)$_SESSION['acc_id']."'");
	mysqli_query($sqlServ, "INSERT INTO player.item_award (pid, login, vnum, count, given_time, why, socket0, socket1, socket2, mall) VALUES ('".(int)$_SESSION['acc_id']."','".$_SESSION['id']."','".(int)$won['vnum']."','".(int)$won['count']."','$data', 'ItemShop Wheel', '".(int)$won['socket0']."', '".(int)$won['socket1']."', '".(int)$won['socket2']."','1')");
	mysqli_query($sqlServ, "INSERT INTO itemshop.wheel_spin_log (buyer_name, level, prize_name, vnum_icon, date_of_spin) VALUES ('".$_SESSION['id']."', 1, '".mysqli_real_escape_string($sqlServ, $won['name_item'])."', '".$won['vnum_icon']."', '$data')");

	$newBalance = $balance - WHEEL_SPIN_COST;
	echo json_encode(array(
		'ok' => true,
		'position' => $position,
		'name' => $won['name_item'],
		'desc' => $won['item_desc'],
		'count' => (int)$won['count'],
		'icon' => $won['vnum_icon'],
		'jackpot' => (bool)$won['is_jackpot'],
		'balance' => $newBalance,
	));
