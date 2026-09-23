<?php
	// Kolo Losu, poziom 1 - bez systemu kluczy/poziomow (na zyczenie), zbudowane
	// na gotowych assetach z oryginalnego szablonu (css/wheel.css, img/spinner.jpg
	// - sprite 16 klatek po 312px, kazda klatka to inna podswietlona cwiartka
	// kola). Zakrecenie idzie przez ten sam mechanizm dostawy co zwykly zakup
	// (player.item_award), patrz pages/wheel_spin.php.
	//
	// Stara wbudowana przegladarka w kliencie potrafi zapamietac cala te
	// strone tak, jakby byla statyczna - przez co swiezo wylosowany na
	// serwerze zestaw nigdy nie docieral na ekran. Zabraniamy jej tego
	// jawnie.
	header("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
	header("Pragma: no-cache");
	$isPreview = isset($_GET['preview']);
	if(!isset($_SESSION['id']) && !$isPreview) {
		header("Location: ?s=login");
		exit;
	}
	if ($isPreview) {
		$info = array('cash' => 0, 'mileage' => 0);
	} else {
		$coninfo = mysqli_query($sqlServ, "SELECT * FROM account.account where login='".$_SESSION['id']."'");
		$info = mysqli_fetch_array($coninfo);
	}
	$balance = (int)$info['cash'];

	// Pula ma 30 mozliwych nagrod (24 zwykle + 6 glownych); kolo pokazuje 16
	// naraz (14 zwyklych + 2 glowne), wylosowane od nowa przy kazdym wejsciu
	// na strone, zeby nie stac cieagle na tych samych. Zapamietujemy dokladnie
	// KTORE 16 wyszlo w tej sesji, zeby zakrecenie (wheel_spin.php) losowalo
	// z tego samego zestawu, ktory gracz widzi na ekranie.
	//
	// Grafika kola ma dwa specjalne, czerwono podswietlone pola - na samej
	// gorze (pozycja 1) i na samym dole (pozycja 9), naprzeciwlegle - tam
	// zawsze ladujak dwie wylosowane nagrody glowne, reszta (zwykle) idzie
	// losowo na pozostale 14 pol.
	$mainRes = mysqli_query($sqlServ, "SELECT id FROM itemshop.wheel_prizes WHERE level=1 AND is_jackpot=1 ORDER BY RAND() LIMIT 2");
	$mainIds = array();
	while ($r = mysqli_fetch_assoc($mainRes)) { $mainIds[] = (int)$r['id']; }

	$regRes = mysqli_query($sqlServ, "SELECT id FROM itemshop.wheel_prizes WHERE level=1 AND is_jackpot=0 ORDER BY RAND() LIMIT 14");
	$regIds = array();
	while ($r = mysqli_fetch_assoc($regRes)) { $regIds[] = (int)$r['id']; }

	$byPosition = array(); // 1-indexed pozycja -> id
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
	$prizes = array();
	while ($p = mysqli_fetch_assoc($prizeRes)) {
		$prizes[] = $p;
	}
?>
<div id="container">
	<?php include("pages/etc.php") ?>
	<div id="wideMainContent">
		<h1>Koło Losu</h1>
		<div class="dynContent wheel" style="position:relative; margin:0 auto;">
			<h1>Poziom 1</h1>
			<div id="info">Zakrec Kolem Losu za jedyne <b>10 SM</b>!</div>
			<div class="main">
				<div id="spinnerBlank"></div>
				<div id="spinner" style="background-position:0 0;"></div>
				<div id="wheel" class="clockwise">
					<?php foreach ($prizes as $i => $p) {
						$pos = $i + 1;
					?>
					<div class="reward" id="pos<?php echo $pos; ?>" style="display:block">
						<img src="img/item/<?php echo $p['vnum_icon']; ?>.png" title="<?php echo htmlspecialchars($p['name_item']); ?>" onerror="this.src='img/error.png';" />
					</div>
					<?php } ?>
				</div>
				<a id="spinButton" href="javascript:void(0)" onclick="wheelSpin()">Zakr&#281;&#263;!</a>
			</div>
			<?php if (!$isPreview) { ?>
			<div class="stageInfo">
				<p>Masz <b><span id="wheelBalance"><?php echo $balance; ?></span></b> SM.</p>
			</div>
			<?php } ?>
			<p class="back"><a href="?s=home">&laquo; Wroc do sklepu</a></p>
		</div>

		<div id="wheelRewardOverlay" style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.6); z-index:9999;">
			<div id="reward" style="position:absolute; top:50%; left:50%; transform:translate(-50%,-50%); width:220px;">
				<h1>Gratulacje!</h1>
				<h2>Fortuna wybra&#322;a dla Ciebie nast&#281;puj&#261;c&#261; nagrod&#281;:</h2>
				<img id="rewardIcon" src="" />
				<p id="rewardName"></p>
				<p id="rewardDesc"></p>
				<div onclick="document.getElementById('wheelRewardOverlay').style.display='none'; wheelReroll();">Dalej</div>
				<a href="javascript:void(0)" onclick="document.getElementById('wheelRewardOverlay').style.display='none'; wheelReroll(); return false;">W ten spos&#243;b otrzymasz swoj&#261; nagrod&#281;.</a>
			</div>
		</div>

		<!-- Osobny, bogatszy popup wylacznie dla nagrody glownej (is_jackpot=1
		w wheel_prizes) - styl niestandardowy, bo szablon nie mial gotowej
		dwukolumnowej wersji tego okna, tylko #reward (jednokolumnowe). -->
		<div id="wheelJackpotOverlay" style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.6); z-index:9999;">
			<div id="jackpot">
				<div id="jackpotVisual">
					<img id="jackpotIcon" src="" />
					<span id="jackpotBadge" style="display:none"></span>
				</div>
				<div id="jackpotText">
					<h1>Poca&#322;owa&#322;a Ci&#281; Fortuna!</h1>
					<p class="jackpotLead">Ta niesamowita g&#322;&oacute;wna nagroda nale&#380;y teraz do Ciebie:</p>
					<h2 id="jackpotName"></h2>
					<p id="jackpotDesc"></p>
					<div class="jackpotFooter">
						<span class="jackpotNote">&#9432; W ten spos&#243;b otrzymasz swoj&#261; nagrod&#281;.</span>
						<a href="javascript:void(0)" id="jackpotPlay" onclick="document.getElementById('wheelJackpotOverlay').style.display='none'; wheelReroll();">Graj dalej</a>
					</div>
					<a href="javascript:void(0)" class="jackpotAgain" onclick="document.getElementById('wheelJackpotOverlay').style.display='none'; wheelReroll(); return false;">&#10022; Poka&#380; ponownie ko&#322;o</a>
				</div>
			</div>
		</div>
		<div class="endContent"></div>
	</div>
</div>
<style>
#jackpot {
	position:absolute; top:50%; left:50%; transform:translate(-50%,-50%);
	width:560px; min-height:280px; background:#ded2a0; border:1px solid #a08c50;
	border-radius:2px; box-shadow:0 4px 20px rgba(0,0,0,0.5);
	display:flex; font-family:Georgia, "Times New Roman", serif; color:#3a2e1a;
}
#jackpotVisual {
	position:relative; width:220px; flex:0 0 220px; display:flex;
	align-items:center; justify-content:center; border-right:1px solid #c9b978;
}
#jackpotVisual img { max-width:170px; max-height:170px; }
#jackpotBadge {
	position:absolute; left:14px; bottom:14px; color:#f4c542; font-weight:bold;
	font-size:34px; text-shadow:0 0 6px #000, 0 0 2px #000; font-family:Arial,sans-serif;
}
#jackpotText { flex:1; padding:22px 24px; }
#jackpotText h1 { margin:0 0 10px; font-size:26px; font-weight:normal; }
#jackpotText p.jackpotLead { margin:0 0 14px; font-size:13px; }
#jackpotText h2 { margin:0 0 8px; font-size:19px; font-weight:normal; }
#jackpotText #jackpotDesc { margin:0 0 16px; font-size:13px; line-height:1.4; max-height:90px; overflow-y:auto; }
.jackpotFooter { display:flex; align-items:center; justify-content:space-between; margin-top:10px; }
.jackpotNote { font-size:12px; color:#5a4d30; }
#jackpotPlay {
	display:inline-block; background:#8c1c1c; color:#fff; font-weight:bold;
	text-decoration:none; padding:10px 22px; border-radius:2px;
}
#jackpotPlay:hover { background:#a52424; }
a.jackpotAgain {
	display:block; margin-top:14px; font-size:12px; color:#5a4d30; text-decoration:underline;
}
</style>
<script>
// Ten skrypt jedzie zawsze, takze w trybie podgladu (?preview=1, tak wchodzi
// sie tu z iframe na m2singleplayer.pl) - inaczej przycisk wisi z onclick na
// niezdefiniowanej funkcji i klikniecie robi dokladnie nic, bez zadnego bledu
// widocznego dla uzytkownika.
var WHEEL_IS_PREVIEW = <?php echo $isPreview ? 'true' : 'false'; ?>;
// W podgladzie (iframe na m2singleplayer.pl, nikt nie jest zalogowany) kolo
// nadal sie kreci i pokazuje wygrana - probnie, bez realnego przyznania
// (patrz wheel_spin_trial.php: brak logowania, brak zmiany salda, brak
// wpisu do item_award).
var WHEEL_SPIN_ENDPOINT = WHEEL_IS_PREVIEW ? 'wheel_spin_trial.php' : 'wheel_spin.php';
var WHEEL_SPINNING = false;
var WHEEL_WATCHDOG = null;

// W kliencie gry (stara wbudowana przegladarka, brak fetch/Promise - stad
// "Brak definicji fetch" w oknie bledu skryptu) trzeba jechac na golym
// XMLHttpRequest, bez zadnych nowszych API. requestAnimationFrame tez moze
// nie istniec, wiec ma prosty fallback na setTimeout.
var wheelRAF = window.requestAnimationFrame || window.msRequestAnimationFrame || function(cb){
	return setTimeout(function(){ cb(new Date().getTime()); }, 16);
};

function wheelSetText(el, text) {
	if (!el) return;
	if (typeof el.textContent !== 'undefined') el.textContent = text;
	else el.innerText = text;
}

// Podmienia 16 ikon na kole na nowo wylosowany zestaw (patrz
// wheel_reroll.php) bez przeladowania calej strony - wolane po zamknieciu
// okienka z wygrana.
function wheelReroll(retriesLeft) {
	if (typeof retriesLeft === 'undefined') retriesLeft = 2;
	xhrPost('wheel_reroll.php', function(data){
		if (!data.ok || !data.items || data.items.length < 16) {
			if (retriesLeft > 0) { wheelReroll(retriesLeft - 1); }
			return;
		}
		for (var i = 0; i < data.items.length; i++) {
			var pos = i + 1;
			var wrap = document.getElementById('pos' + pos);
			if (!wrap) continue;
			var img = wrap.getElementsByTagName('img')[0];
			if (!img) continue;
			img.src = 'img/item/' + data.items[i].icon + '.png';
			img.title = data.items[i].name;
		}
	}, function(){
		// Nieudane polaczenie - sprobuj jeszcze raz zamiast po cichu
		// zostawiac stare ikony (to byl powod "nie zawsze sie odswieza").
		if (retriesLeft > 0) { wheelReroll(retriesLeft - 1); }
	});
}

// Miga polem nagrody, na ktorym kolo sie zatrzymalo, zanim pokaze sie popup
// z wygrana - bez CSS3 @keyframes (stara wbudowana przegladarka w kliencie
// tego nie odpali), wiec zwykle przelaczanie visibility na setIntervalu.
function wheelBlinkPrize(pos, onDone) {
	var el = document.getElementById('pos' + pos);
	if (!el) { onDone(); return; }
	var toggles = 0;
	var maxToggles = 6; // 3 pelne mrugniecia
	var iv = setInterval(function(){
		el.style.visibility = (toggles % 2 === 0) ? 'hidden' : 'visible';
		toggles++;
		if (toggles >= maxToggles) {
			clearInterval(iv);
			el.style.visibility = 'visible';
			onDone();
		}
	}, 150);
}

function xhrPost(url, onSuccess, onError) {
	var xhr;
	try { xhr = new XMLHttpRequest(); }
	catch (e) {
		try { xhr = new ActiveXObject('Msxml2.XMLHTTP'); }
		catch (e2) {
			try { xhr = new ActiveXObject('Microsoft.XMLHTTP'); }
			catch (e3) { onError(); return; }
		}
	}
	xhr.onreadystatechange = function() {
		if (xhr.readyState !== 4) return;
		if (xhr.status < 200 || xhr.status >= 300) { onError(); return; }
		var data;
		try { data = JSON.parse(xhr.responseText); }
		catch (e) { onError(); return; }
		onSuccess(data);
	};
	xhr.open('POST', url, true);
	xhr.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
	xhr.send(null);
}

// Odblokowuje przycisk bezwarunkowo - wolane z kazdej sciezki wyjscia
// (sukces, blad, wyjatek, watchdog) zeby WHEEL_SPINNING nigdy nie zostalo
// zatrzasniete na true. To byl realny bug: dlugi lancuch setTimeout w
// animateSpin potrafil zostac zdlawiony przez przegladarke (karta w tle/bez
// fokusu), animacja nigdy nie konczyla onDone, i kazde kolejne klikniecie
// bylo cicho ignorowane az do przeladowania strony.
function wheelUnlock() {
	WHEEL_SPINNING = false;
	var btn = document.getElementById('spinButton');
	if (btn) btn.style.opacity = '1';
	if (WHEEL_WATCHDOG) { clearTimeout(WHEEL_WATCHDOG); WHEEL_WATCHDOG = null; }
}

function wheelSpin() {
	if (WHEEL_SPINNING) return;
	WHEEL_SPINNING = true;
	var btn = document.getElementById('spinButton');
	btn.style.opacity = '0.5';
	// Siatka bezpieczenstwa: cokolwiek pojdzie nie tak (animacja zdlawiona,
	// nieoczekiwany wyjatek), przycisk odblokuje sie sam najpozniej po 12s
	// zamiast zostac zablokowany do przeladowania strony.
	WHEEL_WATCHDOG = setTimeout(function(){
		WHEEL_WATCHDOG = null;
		wheelUnlock();
	}, 16000);

	xhrPost(WHEEL_SPIN_ENDPOINT, function(data){
		if (!data.ok) {
			alert(data.error || 'Nie udalo sie zakrecic kolem.');
			wheelUnlock();
			return;
		}
		animateSpin(data.position, function(){
			wheelBlinkPrize(data.position, function(){
				setTimeout(function(){
				try {
					wheelSetText(document.getElementById('wheelBalance'), data.balance);
					wheelSetText(document.getElementById('headerCashValue'), data.balance);
					if (data.jackpot) {
						// Duracja (np. "(72h)") nie jest jeszcze czyms, co ta pula
						// nagrod ma - odznaka pokazuje sie tylko, gdy backend kiedys
						// zacznie ja zwracac (data.duration).
						document.getElementById('jackpotIcon').src = 'img/item/' + data.icon + '.png';
						var badge = document.getElementById('jackpotBadge');
						if (data.duration) { wheelSetText(badge, data.duration); badge.style.display = 'block'; }
						else { badge.style.display = 'none'; }
						wheelSetText(document.getElementById('jackpotName'), data.count + 'x ' + data.name);
						wheelSetText(document.getElementById('jackpotDesc'), data.desc || '');
						document.getElementById('wheelJackpotOverlay').style.display = 'block';
					} else {
						document.getElementById('rewardIcon').src = 'img/item/' + data.icon + '.png';
						wheelSetText(document.getElementById('rewardName'), data.count + 'x ' + data.name);
						wheelSetText(document.getElementById('rewardDesc'), data.desc || '');
						document.getElementById('wheelRewardOverlay').style.display = 'block';
					}
				} finally {
					wheelUnlock();
				}
				}, 1500);
			});
		});
	}, function(){
		alert('Blad polaczenia. Sprobuj ponownie.');
		wheelUnlock();
	});
}

// Sprite: 16 klatek po 312px, klatka N (0-indeks) = podswietlona pozycja N+1.
// Liczone od up&#322;ywu prawdziwego czasu (Date.now()), nie od liczby odpalen
// setTimeout - karta w tle dlawi/wstrzymuje timery, ale po jej odblokowaniu
// animacja od razu wie, ile czasu naprawde uplynelo i wskakuje na wlasciwa
// klatke zamiast zostac uwieziona w polowie starego, zbyt wolnego lancucha.
function animateSpin(finalPosition, onDone) {
	var spinner = document.getElementById('spinner');
	var totalFrames = 16 * 2 + (finalPosition - 1); // pelne okrazenia + docelowa klatka
	var duration = 5000 + Math.floor(Math.random() * 2000); // ms, calego kretu - losowo 5-7s na zyczenie
	var start = new Date().getTime();
	var done = false;
	var iv = null;

	// Rozklad czasu miedzy kolejnymi klatkami rosnie LINIOWO (prosta
	// progresja arytmetyczna) zamiast wedlug krzywej potegowej. Krzywa
	// potegowa (ease-out) ma predkosc dazaca do zera tuz przed samym
	// koncem - przy tylko 16 dyskretnych klatkach spritu to wygladalo jak
	// realne zatrzymanie sie na dobry ulamek sekundy, a potem nagly skok o
	// kilka klatek naraz, kiedy postep w koncu doganial prog. Liniowy
	// rozklad nigdy nie zwalnia az do zera - ostatni odstep jest tylko
	// kilka razy dluzszy od pierwszego, wiec kazda klatka, wlacznie z
	// ostatnia, jest osobno widoczna.
	var n = totalFrames; // liczba odstepow (klatek jest n+1: 0..totalFrames)
	var ratio = 22; // ostatni odstep jest ~22x dluzszy od pierwszego
	var d0 = (2 * duration) / (n * (1 + ratio));
	var dLast = ratio * d0;
	var cum = [0];
	for (var i = 1; i <= n; i++) {
		var di = d0 + (dLast - d0) * (i - 1) / (n - 1);
		cum.push(cum[i - 1] + di);
	}
	// male zaokraglenia przy sumowaniu - przeskaluj tak, zeby ostatni prog
	// dokladnie pokrywal sie z duration.
	var scale = duration / cum[n];
	for (var i = 0; i <= n; i++) { cum[i] = cum[i] * scale; }

	var finish = function() {
		if (done) return;
		done = true;
		if (iv) { clearInterval(iv); iv = null; }
		spinner.style.backgroundPosition = '0 -' + (((finalPosition - 1) % 16) * 312) + 'px';
		if (onDone) onDone();
	};
	// setInterval zamiast lancucha requestAnimationFrame/setTimeout, ktory
	// sam siebie ponownie zbroil: w starej wbudowanej przegladarce w kliencie
	// (bez requestAnimationFrame - polyfill lecial na setTimeout) to potrafilo
	// utknac w polowie. setInterval ustawia sie RAZ i silnik przegladarki sam
	// pilnuje kolejnych odpalen; kazdy tick liczy postep od prawdziwego
	// uplywu czasu (Date.getTime()), wiec nawet spozniony tick trafia na
	// wlasciwa klatke.
	var curFrame = 0;
	var tick = function() {
		if (done) return;
		var elapsed = new Date().getTime() - start;
		while (curFrame < n && elapsed >= cum[curFrame + 1]) {
			curFrame++;
		}
		spinner.style.backgroundPosition = '0 -' + ((curFrame % 16) * 312) + 'px';
		if (elapsed >= duration) {
			finish();
		}
	};
	iv = setInterval(tick, 50);
	tick();
	// Siatka bezpieczenstwa, gdyby cos jednak calkowicie przestalo dzialac.
	setTimeout(finish, duration + 1500);
}
</script>
