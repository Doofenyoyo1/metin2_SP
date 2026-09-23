<?php
	$isPreview = isset($_GET['preview']);
	$pv = $isPreview ? '&preview=1' : '';
	if(isset($_SESSION['id']) || $isPreview) {
		if (!$isPreview) {
			$coninfo = mysqli_query($sqlServ, "SELECT * FROM account.account where login='".$_SESSION['id']."'");
			$info = mysqli_fetch_array($coninfo);
		}
?>
<div id="header">
	<?php if (!$isPreview) { ?>
	<div class="boxSigns">
		<span class="heading">Smocze Znaki (SZ):</span>
		<span class="marksValue"><?php echo (int)$info['mileage'];?></span>
	</div>
	<div class="boxCoins">
		<span class="heading">Smocze Monety (SM):</span>
		<span class="coinsValue" id="headerCashValue"><?php echo (int)$info['cash'];?></span>
	</div>
	<?php } ?>
</div>
<?php if (!$isPreview) { ?>
<div class="userdataDiv">
	<a title="Historia zakupow" href="?s=userdata" class="tip userdataIcon"></a>
</div>
<?php } ?>

<ul id="breadcrumb">
	<li><a href="?s=home<?php echo $pv; ?>">Strona glowna</a></li>
	<?php if (!$isPreview) { ?>
	<li><a>-</a></li>
	<li><a href="?s=logout">Wyloguj</a></li>
	<?php } ?>
</ul>
<div id="sidebar1">
	<!--<div id="search">
		<form action="" method="post" name="searchForm" onsubmit="return trySubmit()">
			<input type="text" value="Cauta termen" class="type" name="searchString" onfocus="searchFocusGained()" onblur="searchFocusLost()" maxlength="42" /><input type="submit" value="" class="send" />
		</form>
	</div>-->
	<ul id="mainMenu">
		<li class="wheelMenuItem"><a class="wheelMenuLink" href="?s=wheel<?php echo $pv; ?>" title="Koło Losu">Koło Losu</a></li>
		<?php
		$get_category = mysqli_query($sqlServ, "SELECT * FROM itemshop.ishop_category ORDER BY id");
		while($category = mysqli_fetch_object($get_category)) {
			echo '<li><a href="?s=category&id='.$category->id.$pv.'" title="'.$category->name.'">'.$category->name.'</a></li>';
		}
		?>
	</ul>
	<?php if (!$isPreview) { ?>
	<?php echo'<br /><font color="#996600;"> Uzytkownik:<br /> '.htmlspecialchars($_SESSION['id']).'</font>';?>
	<?php } ?>
</div>
<?php
	} else {
?>
<div id="header"></div>
<div id="breadcrumb">&nbsp;</div>
<div id="sidebar1"></div>
<?php
	}
?>
