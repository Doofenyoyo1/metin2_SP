<div id="container">
	<?php include("pages/etc.php") ?>
	<div id="mainContent">
		<h1>Wyloguj</h1>
		<div class="dynContent">
			<div class="item" id="confirmBox">
				<div class="itemDesc confirmDesc">
					<div class="thumbnailBgSmall"><img src="img/error.png" width="63px" height="63px" /></div>
					<p>
						<span class="confirmTitle">Wyloguj</span><br  />
						<?php
							// unset($_SESSION['id']);
							session_destroy();
							echo'<meta http-equiv="refresh" content="3; URL=?s=home"> ';
						?>
						<span>Zostales wylogowany, poczekaj 3 sekundy.</span>
					</p>
					<br class="clearfloat" />
				</div>
			</div>
		</div>
		<div class="endContent"></div>
	</div>
</div>
