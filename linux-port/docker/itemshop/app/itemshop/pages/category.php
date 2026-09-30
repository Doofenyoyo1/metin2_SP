<?php
	if (!isset($_GET['id'])) {
		die("Musisz wybrac kategorie.");
	}
	$id = intval($_GET['id']);
	$isPreview = isset($_GET['preview']);
	$pv = $isPreview ? '&preview=1' : '';
	if(!isset($_SESSION['id']) && !$isPreview) {
		header("Location: ?s=login");
		exit;
	} else {
?>
<div id="container">
	<?php include("pages/etc.php") ?>
	<div id="mainContent">
		<?php
			$get_cat = mysqli_query($sqlServ, "SELECT name FROM itemshop.ishop_category WHERE id=" . $id);
			$cat = $get_cat ? mysqli_fetch_assoc($get_cat) : null;
		?>
		<h1><?php echo $cat ? htmlspecialchars($cat['name']) : 'Lista przedmiotow'; ?></h1>
		<div class="dynContent" style="position:relative">
			<?php
				// Paging (custom-patches/itemshop/category_paging.patch): the GF26
				// categories hold hundreds of offers, too many for one page in the
				// client's embedded browser.
				$perPage = 30;
				// Only what the character the shop was opened from can wear: the
				// item's own anti-flags (bits 2-5 class, 0 female / 1 male), the
				// same test the game makes when equipping. "Show all" for buying
				// for another character of the account (the item goes to the
				// account's item-shop storage either way).
				if (isset($_GET['all']))
					$_SESSION['ishop_all'] = intval($_GET['all']) ? 1 : 0;
				$filterSql = '';
				$filterInfo = '';
				if (!$isPreview && !empty($_SESSION['pid'])) {
					$jr = mysqli_query($sqlServ, "SELECT job FROM player.player WHERE id=" . intval($_SESSION['pid']));
					$jrow = $jr ? mysqli_fetch_assoc($jr) : null;
					if ($jrow) {
						$job = (int)$jrow['job'];
						$classNames = array('Wojownik', 'Ninja', 'Sura', 'Szaman');
						$isMale = in_array($job, array(0, 2, 5, 7));
						$mask = (4 << ($job % 4)) | ($isMale ? 2 : 1);
						$who = $classNames[$job % 4] . ($isMale ? ' (mężczyzna)' : ' (kobieta)');
						if (empty($_SESSION['ishop_all'])) {
							// A weapon skin also needs the weapon it goes on (value3 = weapon
							// subtype, checked by the game on equip): warrior sword/two-handed,
							// ninja sword/dagger/bow, sura sword, shaman bell/fan.
							$weapons = array('0,3', '0,1,2', '0', '4,5');
							$filterSql = " AND NOT EXISTS (SELECT 1 FROM world.item_proto ip WHERE ip.vnum=itemshop.ishop_items.vnum AND ((ip.antiflag & " . $mask . ") <> 0"
								. " OR (ip.type=28 AND ip.subtype=4 AND ip.value3 NOT IN (" . $weapons[$job % 4] . "))))";
							$filterInfo = 'Przedmioty dla postaci: <b>' . $who . '</b> &ndash; <a href="?s=category&id=' . $id . '&all=1">pokaż wszystkie</a>';
						} else {
							$filterInfo = 'Wszystkie przedmioty &ndash; <a href="?s=category&id=' . $id . '&all=0">pokaż tylko dla: ' . $who . '</a>';
						}
					}
				}
				if ($filterInfo !== '')
					echo '<div class="catFilter" style="clear:both;text-align:center;padding:6px 0;">' . $filterInfo . '</div>';
				$cnt = mysqli_query($sqlServ, "SELECT COUNT(*) AS n FROM itemshop.ishop_items WHERE category=" . $id . $filterSql);
				$total = $cnt ? (int)mysqli_fetch_assoc($cnt)['n'] : 0;
				$pages = max(1, (int)ceil($total / $perPage));
				$pg = isset($_GET['p']) ? max(1, min($pages, intval($_GET['p']))) : 1;
				$pager = '';
				if ($pages > 1) {
					$pager .= '<div class="catPager" style="clear:both;text-align:center;padding:8px 0;">';
					if ($pg > 1)
						$pager .= '<a href="?s=category&id=' . $id . '&p=' . ($pg - 1) . $pv . '">&laquo; Poprzednia</a> ';
					for ($i = 1; $i <= $pages; $i++) {
						if ($i == $pg)
							$pager .= ' <b>' . $i . '</b> ';
						elseif ($i == 1 || $i == $pages || abs($i - $pg) <= 3)
							$pager .= ' <a href="?s=category&id=' . $id . '&p=' . $i . $pv . '">' . $i . '</a> ';
						elseif (abs($i - $pg) == 4)
							$pager .= ' &hellip; ';
					}
					if ($pg < $pages)
						$pager .= ' <a href="?s=category&id=' . $id . '&p=' . ($pg + 1) . $pv . '">Następna &raquo;</a>';
					$pager .= '</div>';
				}
				echo $pager;
				$get_item = mysqli_query($sqlServ, "SELECT * FROM itemshop.ishop_items where category=" . $id . $filterSql
					. " ORDER BY id LIMIT " . $perPage . " OFFSET " . (($pg - 1) * $perPage));
				while($item = mysqli_fetch_object($get_item)) {
			?>
			<div class="item">
				<div class="itemDesc">
					<div class="thumbnailBgSmall">
						<a href="?s=detail&id=<?php echo $item->id . $pv; ?>" title="Wiecej informacji" class="openinformation">
							<img src="img/item/<?php echo $item->vnum_icon; ?>.png" onerror="this.src='img/error.png';" width="63px" height="63px" alt="Wiecej informacji"/>
						</a>
					</div>
					<p>
						<a href="?s=detail&id=<?php echo $item->id . $pv; ?>" title="Wiecej informacji" class="openinformation">
							<span class="itemTitle"><?php echo $item->name_item; ?></span>
						</a>
						<span class="line"></span>
						<?php
						if (empty($item->desc))
								{
						?>
						<span>Ten przedmiot nie ma opisu.</span>
						<?php
						} else { ?>
						<?php echo $item->desc; ?>
						<?php
						}
						?>
					</p>
				</div>
				<div class="purchaseOptionsWrapper">
					<div class="itemPrice">
						<div class="priceValue">Ilosc: <?php echo $item->count; ?> - Cena:<span class="price">&nbsp;<?php echo $item->price; ?> <?php echo $item->currency === 'mileage' ? 'SZ' : 'SM'; ?></span></div>
					</div>
					<a href="?s=detail&id=<?php echo $item->id . $pv; ?>" title="Wiecej informacji" class="purchaseInfo openinformation">Szczegoly</a>
					<br class="clearfloat" />
				</div>
			</div>
			<?php
			}
			echo $pager;
			?>
		</div>
		<div class="endContent"></div>
	</div>
</div>
<?php
}
?>
