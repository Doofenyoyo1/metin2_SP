<?php
	if (!isset($_GET['id'])) {
		die("Musisz wybrac przedmiot.");
	}
	$id = intval($_GET['id']);
	$isPreview = isset($_GET['preview']);
	if(!isset($_SESSION['id']) && !$isPreview) {
		header("Location: ?s=login");
		exit;
	} else {
		if ($isPreview) {
			$info = array('cash' => 0, 'mileage' => 0);
		} else {
			$coninfo = mysqli_query($sqlServ, "SELECT * FROM account.account where login='".$_SESSION['id']."'");
			$info = mysqli_fetch_array($coninfo);
		}
		$get_item = mysqli_query($sqlServ, "SELECT * FROM itemshop.ishop_items where id=" . $id);
		$item = mysqli_fetch_array($get_item);
?>
<div id="fancybox-content" style="border-width: 0px; width: 540px; height: 500px;">
	<div style="width:540px;height:500px;overflow: hidden;position:relative;">
		<?php
			$currency = ($item['currency'] === 'mileage') ? 'mileage' : 'cash';
			$currencyLabel = ($currency === 'mileage') ? 'Smocze Znaki' : 'Smocze Monety';
			$balance = (int)$info[$currency];
		?>
		<h1>Zakup <?echo $items; ?></h1>
		<?php
			// Tryb podgladu: pokaz DOKLADNIE ten sam ekran sukcesu co przy prawdziwym
			// zakupie w grze, ale bez zadnego zapisu do bazy - zaden przedmiot ani
			// waluta nigdzie nie sa przydzielane.
			if($isPreview || $balance >= $item['price']){
			if (!$isPreview) {
			$data = date('Y-m-d H:i:s');
			mysqli_query($sqlServ, "INSERT INTO itemshop.ishop_log (buyer_name, item_name, date_of_buy, vnum_icon) VALUES ('".$_SESSION['id']."', '".$item['name_item']."', '$data', '".$item['vnum_icon']."')");
			mysqli_query($sqlServ, "UPDATE account.account SET `$currency` = `$currency` - ".$item['price']." WHERE `id` = '".$_SESSION['acc_id']."'");
			// Kazdy zakup za Smocze Monety zwraca 10% ceny w Smoczych Znakach -
			// to samo zrodlo dla kategorii "Przedmioty za Znaki".
			if ($currency === 'cash') {
				$reward = max(1, (int)floor($item['price'] / 10));
				mysqli_query($sqlServ, "UPDATE account.account SET `mileage` = `mileage` + ".$reward." WHERE `id` = '".$_SESSION['acc_id']."'");
			}
			// Paczki (np. Duzy Pakiet Startowy) daja kilka roznych przedmiotow za
			// jeden zakup - jesli ishop_bundle_items ma wiersze dla tego itemu,
			// dostarcz KAZDY z nich; w przeciwnym razie zachowaj sie jak zawsze
			// (jeden vnum/count/sockets z samego ishop_items).
			$bundleRes = mysqli_query($sqlServ, "SELECT vnum, count, socket0, socket1, socket2 FROM itemshop.ishop_bundle_items WHERE item_id=" . (int)$item['id']);
			$bundleRows = array();
			while ($bundleRes && ($b = mysqli_fetch_assoc($bundleRes))) {
				$bundleRows[] = $b;
			}
			if (count($bundleRows) === 0) {
				$bundleRows[] = array('vnum' => $item['vnum'], 'count' => $item['count'],
					'socket0' => $item['socket0'], 'socket1' => $item['socket1'], 'socket2' => $item['socket2']);
			}
			foreach ($bundleRows as $b) {
				mysqli_query($sqlServ, "INSERT INTO player.item_award (pid, login, vnum, count, given_time, why, socket0, socket1, socket2, mall) VALUES ('".$_SESSION['acc_id']."','".$_SESSION['id']."','".$b['vnum']."','".$b['count']."','$data', 'ItemShop Buy', '".$b['socket0']."', '".$b['socket1']."', '".$b['socket2']."','1')");
			}
			}
		?>
		<div class="dynContent">
			<div id="confirmBox" class="item">
				<div class="itemDesc confirmDesc">
					<div class="thumbnailBgSmall">
						<img width="63px" height="63px" src="img/7227be80292ec244a17496ca9b2528.png"></img>
					</div>
					<p>
						<span class="confirmTitle">Zakup zakonczony sukcesem</span>
						<?php if ($isPreview) { ?>
						<br />(Podglad - nic nie zostalo przydzielone.)</br>
						<?php } else { ?>
						<br />Przedmiot zostal dodany do Twojej skrzynki (item_award)!</br>
						<?php } ?>
						</br><b>Strona odswiezy sie za 5 sekund.
						<!--</b><meta http-equiv="refresh" content="10;">-->
						<script>
							setTimeout(function(){
							   window.location.reload(1);
							}, 5000);
						</script>
					</p>
					<br class="clearfloat"></br>
				</div>
			</div>
		</div>
		<?php
			} else {
		?>
		<div class="dynContent">
			<div id="confirmBox" class="item">
				<div class="itemDesc confirmDesc">
					<div class="thumbnailBgSmall">
						<img width="63px" height="63px" src="img/error.png"></img>
					</div>
					<p>
						<span class="confirmTitle"><font color="red">Zakup nieudany</font></span>
						<br />Nie masz wystarczajaco waluty: <?php echo $currencyLabel; ?>!
						<br />Masz <?php echo $balance; ?>, przedmiot kosztuje <?php echo $item['price'];?> (<?php echo $currencyLabel; ?>).</br>
					</p>
					<br class="clearfloat"></br>
				</div>
			</div>
		</div>
		<?php
			}
		?>
		<?php
			if(!$isPreview && $balance >= $item['price']){
		?>
		<div class="hint">
			<div class="itemDesc messageDesc">
				<p>
					<span class="hintTitle">Uwaga</span><br />
					</br>Przedmiot otrzymasz w grze poprzez system item_award (magazyn) - wejdz na serwer aby go odebrac.
				</p>
				<br class="clearfloat"></br>
			</div>
		</div>
		<?php
			}
		?>
	</div>
</div>
<a id="fancybox-close" style="display: inline;"></a>
<div id="fancybox-title" style="display: block;"></div>
<?php
	}
?>
