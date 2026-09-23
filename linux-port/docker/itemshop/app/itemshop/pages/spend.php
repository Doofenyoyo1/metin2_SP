<?php
	if(!isset($_SESSION['id'])) {
		header("Location: ?s=login");
		exit;
	} else {
?>
<div id="container">
	<?php include("pages/etc.php") ?>
	<div id="mainContent">
		<h1>Doladowanie Smoczych Monet</h1>
		<div class="dynContent" style="position:relative">
				<font color="#996600;" size="3">
				<br /><p>Smocze Monety (SM) zdobywasz grajac na serwerze (questy, wydarzenia).
				Smocze Znaki (SZ) dostajesz automatycznie za kazdy zakup w tym sklepie.<br />
				Skontaktuj sie z administracja, jesli chcesz dowiedziec sie wiecej.
				</p><br />
				</font>
		</div>
		<div class="endContent"></div>
	</div>
</div>
<?php
	}
?>
