Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# A text in the launcher's language: .m2launcher.json's "language", which the
# window and Metin2-Launcher.ps1 put into $env:M2_LAUNCHER_LANGUAGE for every
# module and every action they start. The English one for 'en', otherwise the
# Polish one, which is word for word what the launcher always said. Each
# module keeps its own copy and none exports it.
function UI-Text {
    param([AllowEmptyString()][string]$Pl, [AllowEmptyString()][string]$En)
    if ($env:M2_LAUNCHER_LANGUAGE -eq 'en' -and $En) { return $En }
    return $Pl
}

function Invoke-M2DiagnosticProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FileName,
        [AllowEmptyString()][string]$Arguments = '',
        [ValidateRange(100, 30000)][int]$TimeoutMilliseconds = 3500
    )

    $process = $null
    try {
        $startInfo = [Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $FileName
        $startInfo.Arguments = $Arguments
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $process = [Diagnostics.Process]::Start($startInfo)
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            try { $process.Kill() } catch {}
            return [pscustomobject]@{
                ExitCode = -1
                TimedOut = $true
                Output = (UI-Text 'Polecenie nie odpowiedziało w wyznaczonym czasie.' 'The command did not answer in time.')
            }
        }
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            TimedOut = $false
            Output = ($stdout + $stderr).Trim()
        }
    }
    catch {
        return [pscustomobject]@{
            ExitCode = -1
            TimedOut = $false
            Output = $_.Exception.Message
        }
    }
    finally {
        if ($process) { $process.Dispose() }
    }
}

function Get-M2OneDriveRootFor {
    # The OneDrive folder a path lies in, or nothing. OneDrive moves the
    # Desktop and the Documents into itself (Known Folder Move), so a server
    # unpacked on the Desktop can end up there one day without anybody touching
    # it: Avalach's did between 14:57 and 18:41 on 24 September, and every
    # build after that died on a boost header the build context no longer
    # carried ("forward1_256.hpp: No such file or directory"), five updates in
    # a row, while the diagnostics said the server could start. Docker reads a
    # OneDrive tree through its cloud placeholders, and files go missing from
    # what it sends to the build.
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string]$Path)

    if (-not $Path) { return '' }
    $full = ''
    try { $full = [IO.Path]::GetFullPath($Path).TrimEnd('\') + '\' } catch { return '' }
    $roots = [Collections.Generic.List[string]]::new()
    foreach ($name in @('OneDrive', 'OneDriveConsumer', 'OneDriveCommercial')) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ($value) { $roots.Add([string]$value) }
    }
    try {
        foreach ($account in @(Get-ChildItem -LiteralPath 'HKCU:\Software\Microsoft\OneDrive\Accounts' -ErrorAction Stop)) {
            $props = Get-ItemProperty -LiteralPath $account.PSPath -ErrorAction SilentlyContinue
            if ($props -and ($props.PSObject.Properties.Name -contains 'UserFolder') -and $props.UserFolder) {
                $roots.Add([string]$props.UserFolder)
            }
        }
    }
    catch {}
    foreach ($root in $roots) {
        $prefix = ''
        try { $prefix = [IO.Path]::GetFullPath($root).TrimEnd('\') + '\' } catch { continue }
        if ($prefix.Length -gt 3 -and $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            return $prefix.TrimEnd('\')
        }
    }
    return ''
}

function Get-M2OneDriveRemedy {
    param([AllowEmptyString()][string]$OneDriveRoot = '')

    # Single quotes and -f: PowerShell takes the typographic quotes of the
    # Polish text for string delimiters.
    $where = if ($OneDriveRoot) { ' (' + $OneDriveRoot + ')' } else { '' }
    return ((UI-Text 'Folder serwera leży w OneDrive{0}. Docker nie widzi części plików z folderów OneDrive, więc budowa serwera zatrzymuje się na pliku, którego „nie ma”, choć leży na dysku. Zamknij launcher i przenieś cały folder gry - ten, w którym są foldery Serwer i Klient - poza OneDrive, np. do C:\Metin2 Singleplayer. Potem uruchom Metin2-Launcher-GUI.bat z nowego miejsca (stary skrót na pulpicie wskazuje starą ścieżkę) i kliknij GRAJ. Świat, postacie i boty są w Dockerze i nic z nich nie zginie. Jeśli po przeniesieniu budowa dalej zgłasza brakujący plik, rozpakuj na folder Serwer pełną paczkę serwera, zostawiając swój plik .env.' 'The server folder is in OneDrive{0}. Docker does not see some of the files in OneDrive folders, so the server build stops at a file that "is not there" although it is on the disk. Close the launcher and move the whole game folder - the one holding the Serwer and Klient folders - out of OneDrive, e.g. to C:\Metin2 Singleplayer. Then start Metin2-Launcher-GUI.bat from the new place (the old desktop shortcut points to the old path) and click PLAY. The world, the characters and the bots are in Docker and nothing of them is lost. If the build still reports a missing file after the move, unpack the full server package over the Serwer folder, keeping your .env file.') -f $where)
}

function Get-M2LauncherErrorGuidance {
    param(
        [AllowEmptyString()][string]$Text,
        [AllowEmptyString()][string]$ServerRoot = ''
    )

    $value = [string]$Text
    # The port the failure names. Docker Desktop has said "listen tcp4
    # 127.0.0.1:3306" and "exposing port TCP 127.0.0.1:3306" for a while, where
    # this knew only "listen tcp", and a port it could not read was reported as
    # the panel's 7788: MySQL held 3306 and the advice was to free 7788
    # (Producent Hip Hopu, 27 September). No number is better than a wrong one.
    $port = ''
    foreach ($pattern in @(
            '(?i)(?:bind for|listen(?: (?:tcp|udp)[46]?)?|exposing port (?:tcp|udp))\s+(?:\[[^\]]*\]|[^\s:\[\]]+)?:(?<port>\d{2,5})\b',
            '(?i)(?<port>\d{2,5})\b.{0,80}(?:port is already allocated|address already in use|only one usage of each socket address|jednokrotne u.ycie)')) {
        if ($value -match $pattern) {
            $port = $Matches.port
            break
        }
    }

    # A file the build could not find, in a server folder OneDrive holds
    # (Get-M2OneDriveRootFor): the file is on the disk and not in what Docker
    # was sent, so no reinstall of the same folder helps. Asked first, because
    # the compiler's missing header reads like a broken package.
    if ($ServerRoot -and $value -match '(?i)fatal error: [^\r\n]+: No such file or directory|failed to compute cache key|not found in build context') {
        $oneDrive = Get-M2OneDriveRootFor -Path $ServerRoot
        if ($oneDrive) {
            return [pscustomobject]@{
                Code = 'ONEDRIVE_BUILD_CONTEXT'
                Title = (UI-Text 'Folder serwera jest w OneDrive' 'The server folder is in OneDrive')
                Message = (UI-Text 'Budowa serwera zatrzymała się na pliku, którego Docker nie dostał, choć jest w folderze serwera. Folder leży w OneDrive (zwykle dlatego, że OneDrive przeniósł do siebie Pulpit), a Docker nie widzi części plików z takich folderów. Baza i postęp są w porządku.' 'The server build stopped at a file Docker did not get, although it is in the server folder. The folder is in OneDrive (usually because OneDrive moved the Desktop into itself), and Docker does not see some of the files in such folders. The database and the progress are fine.')
                Remedy = (Get-M2OneDriveRemedy -OneDriveRoot $oneDrive)
            }
        }
    }

    # An engine file a mod rewrote to call into the bot manager for something
    # it never had: an update replaces our files and leaves that one, and the
    # build dies on it at every click - archonek on 18 September, every update
    # from 2.0.74 on, with messenger_manager.cpp and GetCompanionOwner from a
    # fork's companion system, while this function answered with nothing that
    # named the file. Asked first: the compiler's line is the whole answer.
    if ($value -match '(?i)([A-Za-z0-9_]+\.(?:cpp|h)):\d+:\d+: error: [^\r\n]*CPlayerBotManager[^\r\n]*has no member named') {
        $modFile = $Matches[1]
        return [pscustomobject]@{
            Code = 'ENGINE_FILE_FROM_MOD'
            Title = (UI-Text "Plik silnika $modFile pochodzi z innej przeróbki" "The engine file $modFile comes from another mod")
            Message = (UI-Text "Budowa rdzenia gry zatrzymała się na pliku ${modFile}: woła funkcję botów, której w tej wersji nie ma. Ten plik pochodzi z cudzej przeróbki (modu) i aktualizacja go nie podmieniła. Baza i postęp są w porządku." "The game core's build stopped at the file ${modFile}: it calls a bot function this version does not have. The file comes from somebody else's mod, and the update did not replace it. The database and the progress are fine.")
            Remedy = (UI-Text "Przywróć fabryczny plik ${modFile}: skopiuj go z pełnej paczki serwera do folderu serwera, do podfolderu linux-port\docker\game\src\server\game\src, i kliknij ZAINSTALUJ AKTUALIZACJE albo GRAJ. Jeśli nie masz pełnej paczki, dołącz logi do zgłoszenia na GitHubie (ZBIERZ / WYŚLIJ LOGI)." "Put the factory file ${modFile} back: copy it from the full server package into the server folder, to the subfolder linux-port\docker\game\src\server\game\src, and click CHECK FOR UPDATES or PLAY. If you have no full package, attach the logs to a report on GitHub (COLLECT / SEND LOGS).")
        }
    }

    # The launcher's own refusal - the start's preflight, or the update's
    # Assert-ServerPortsFree - names each port a program of Windows' own holds
    # in a line of Get-M2ProgramPortAdvice's, and that line is the remedy. The
    # unknown case below shows the output's last line, which there is only
    # "the database is fine". Case-sensitive: the preflight's own check lines
    # say "port" and are not advice. A launcher set to English words the same
    # advice "Port <n> (<what>) is held by ..." (28 September).
    $heldPorts = @([regex]::Matches($value, 'Port (\d{2,5}) \([^)\r\n]*\) (?:zajmuje|is held by) [^\r\n]+') | ForEach-Object { $_ })
    if ($heldPorts.Count -gt 0) {
        return [pscustomobject]@{
            Code = 'PORT_IN_USE'
            Title = (UI-Text "Port $($heldPorts[0].Groups[1].Value) jest już zajęty" "Port $($heldPorts[0].Groups[1].Value) is already in use")
            Message = (UI-Text 'Inny program trzyma port serwera, więc serwer nie może na nim wystartować. Baza, postacie i ustawienia są w porządku.' 'Another program holds a server port, so the server cannot start on it. The database, the characters and the settings are fine.')
            Remedy = (@($heldPorts | ForEach-Object { $_.Value.Trim() } | Select-Object -Unique) -join [Environment]::NewLine)
        }
    }

    # A port something holds. Asked before the reserved range below: Docker
    # Desktop begins both failures with "ports are not available", and only the
    # reason after it tells them apart - "Only one usage of each socket address"
    # (WSAEADDRINUSE; in Polish "jednokrotne użycie") is a program on the port,
    # "forbidden by its access permissions" is Windows' own reservation. A MySQL
    # on 3306 was told the port was reserved, and which one it was not.
    if ($value -match '(?i)port is already allocated|address already in use|only one usage of each socket address|jednokrotne u.ycie|WSAEADDRINUSE|\b10048\b|failed programming external connectivity|bind for .+ failed|port \d{2,5} (?:jest zajęty|zajmuje|is already in use|is held by)') {
        $holder = ''
        $remedy = (UI-Text 'Kliknij GRAJ albo ZAINSTALUJ AKTUALIZACJE jeszcze raz - launcher sam znajdzie kontener innej instalacji trzymający ten port i zatrzyma go, nie ruszając bazy, wolumenów ani postępu (w wersji konsolowej robi to opcja 21, Zwolnij porty). Samo wyłączenie Docker Desktop nie pomaga: kontenery mają politykę restart=unless-stopped, więc wracają przy każdym starcie silnika i znów zajmują port. Nie usuwaj wolumenów Dockera.' 'Click PLAY or CHECK FOR UPDATES again - the launcher finds the container of another installation that holds this port and stops it, without touching the database, the volumes or the progress (in the console launcher option 21, Free the ports, does it). Just turning Docker Desktop off does not help: the containers have the restart=unless-stopped policy, so they come back at every engine start and take the port again. Do not delete Docker''s volumes.')
        # A program of Windows' own on the port, not a container, is nothing
        # the launcher can stop, and "click GRAJ again" fails the same way:
        # name the program, and the line of .env that moves the port when the
        # game client does not dial it by its own number.
        if ($port -and $ServerRoot) {
            $listener = $null
            try { $listener = Get-M2ListeningProcess -Port ([int]$port) } catch { }
            if ($listener -and $listener.Name -and $listener.Name -notmatch '(?i)^(com\.docker|docker|vpnkit|wslrelay)') {
                $holder = (UI-Text " Trzyma go program $($listener.Name) (PID $($listener.Pid))." " The program $($listener.Name) (PID $($listener.Pid)) holds it.")
                $entry = $null
                try {
                    $entry = @(Get-M2StackHostPorts -ServerRoot $ServerRoot |
                        Where-Object { [int]$_.Port -eq [int]$port }) | Select-Object -First 1
                }
                catch { }
                if ($entry -and -not $entry.ClientFixed -and $entry.Key) {
                    $remedy = ((UI-Text 'Zamknij program {0} (jeśli to usługa, np. MySQL, zatrzymaj ją w Usługach Windows) albo zmień w pliku linux-port\docker\.env wiersz {1}={2} na inny wolny port, np. {3}, i kliknij GRAJ. Baza i postęp są w porządku.' 'Close the program {0} (if it is a service, e.g. MySQL, stop it in Windows Services) or change the line {1}={2} in the file linux-port\docker\.env to another free port, e.g. {3}, and click PLAY. The database and the progress are fine.') -f
                        $listener.Name, $entry.Key, $port, ([int]$port + 1))
                }
                else {
                    $remedy = ((UI-Text 'Zamknij program {0} (Menedżer zadań, karta Szczegóły, Zakończ zadanie; jeśli to usługa, zatrzymaj ją w Usługach Windows) i kliknij GRAJ jeszcze raz. Baza i postęp są w porządku.' 'Close the program {0} (Task Manager, the Details tab, End task; if it is a service, stop it in Windows Services) and click PLAY again. The database and the progress are fine.') -f
                        $listener.Name)
                }
            }
        }
        $title = if ($port) { (UI-Text "Port $port jest już zajęty" "Port $port is already in use") } else { (UI-Text 'Port serwera jest już zajęty' 'A server port is already in use') }
        $which = if ($port) { (UI-Text "portu $port" "the port $port") } else { (UI-Text 'jednego z portów serwera' 'one of the server''s ports') }
        return [pscustomobject]@{
            Code = 'PORT_IN_USE'
            Title = $title
            Message = (UI-Text "Inny program albo druga instalacja serwera używa $which.$holder Launcher nie uruchomi drugiego serwera na tym samym porcie." "Another program or a second installation of the server uses $which.$holder The launcher will not start a second server on the same port.")
            Remedy = $remedy
        }
    }

    # Not a busy port: Windows itself refused the bind. Hyper-V and WSL reserve
    # random port ranges after a restart ("excluded port ranges"), and when
    # 11000 or 13000 falls inside one, compose fails one second after the
    # images are built with a message about access permissions. Five updates
    # in a row went that way for one player before this branch existed.
    if ($value -match '(?i)ports are not available|forbidden by its access permissions|zabroniony przez (?:jego )?uprawnienia|WSAEACCES|\b10013\b') {
        $title = if ($port) { (UI-Text "Windows zarezerwował port $port" "Windows has reserved the port $port") } else { (UI-Text 'Windows zarezerwował port serwera' 'Windows has reserved a server port') }
        $subject = if ($port) { "Port $port" } else { (UI-Text 'Port serwera' 'A server port') }
        return [pscustomobject]@{
            Code = 'PORT_EXCLUDED'
            Title = $title
            Message = (UI-Text "$subject nie jest zajęty przez program - jest w zakresie, który Windows (Hyper-V/WSL) zarezerwował dla siebie po ostatnim restarcie. Docker nie może na nim nasłuchiwać, więc serwer nie wstaje. Pliki serwera i baza są w porządku." "$subject is not held by a program - it is in a range Windows (Hyper-V/WSL) reserved for itself after the last restart. Docker cannot listen on it, so the server does not come up. The server files and the database are fine.")
            Remedy = (UI-Text 'Uruchom PowerShell jako administrator i wykonaj: net stop winnat, potem kliknij GRAJ w launcherze, a gdy serwer wstanie, wykonaj: net start winnat. Zwykle pomaga też zwykły restart Windows. Sprawdzenie zakresów: netsh interface ipv4 show excludedportrange protocol=tcp' 'Start PowerShell as administrator and run: net stop winnat, then click PLAY in the launcher, and once the server is up, run: net start winnat. A plain restart of Windows usually helps too. To see the ranges: netsh interface ipv4 show excludedportrange protocol=tcp')
        }
    }

    if ($value -match '(?i)virtuali[sz]ation support (?:(?:wasn.t |was )?not )?detected|hardware.assisted virtuali[sz]ation|virtuali[sz]ation.*disabled') {
        return [pscustomobject]@{
            Code = 'VIRTUALIZATION_DISABLED'
            Title = (UI-Text 'Wirtualizacja jest wyłączona' 'Virtualization is turned off')
            Message = (UI-Text 'Docker Desktop nie wystartuje, dopóki wirtualizacja procesora nie będzie dostępna dla Windows.' 'Docker Desktop will not start until the processor''s virtualization is available to Windows.')
            Remedy = (UI-Text 'W BIOS/UEFI włącz AMD SVM/AMD-V albo Intel VT-x. Następnie włącz funkcje „Virtual Machine Platform” i „Windows Subsystem for Linux”, uruchom jako administrator: wsl --install, po czym zrestartuj komputer.' 'Turn on AMD SVM/AMD-V or Intel VT-x in the BIOS/UEFI. Then turn on the features "Virtual Machine Platform" and "Windows Subsystem for Linux", run as administrator: wsl --install, and restart the computer.')
        }
    }

    if ($value -match '(?i)there was a problem with wsl|wsl.+(?:error|failed|exit status)|Wsl/Service/|WSL 2 installation is incomplete|windows subsystem for linux.+(?:missing|disabled)') {
        return [pscustomobject]@{
            Code = 'WSL_BROKEN'
            Title = (UI-Text 'WSL 2 wymaga naprawy' 'WSL 2 needs repairing')
            Message = (UI-Text 'Docker Desktop nie może uruchomić swojego środowiska WSL 2.' 'Docker Desktop cannot start its WSL 2 environment.')
            Remedy = (UI-Text 'Otwórz PowerShell jako administrator i wykonaj kolejno: wsl --status, wsl --update oraz wsl --install. Zrestartuj Windows. Jeżeli błąd pozostanie, sprawdź czy w BIOS/UEFI jest włączone AMD SVM/Intel VT-x.' 'Open PowerShell as administrator and run one after another: wsl --status, wsl --update and wsl --install. Restart Windows. If Docker still does not start, check that AMD SVM/Intel VT-x is on in the BIOS/UEFI.')
        }
    }

    if ($value -match '(?i)docker engine did not become ready|cannot connect to the docker daemon|open //./pipe/docker|docker desktop is unable to start|docker api is unavailable') {
        return [pscustomobject]@{
            Code = 'DOCKER_NOT_READY'
            Title = (UI-Text 'Docker Engine nie jest jeszcze gotowy' 'Docker Engine is not ready yet')
            Message = (UI-Text 'Okno Docker Desktop może być otwarte, ale jego silnik nadal startuje albo zatrzymał się na błędzie.' 'The Docker Desktop window may be open, but its engine is still starting or has stopped on an error.')
            Remedy = (UI-Text 'Odczekaj chwilę i spróbuj ponownie. Jeśli status nie zmieni się na „GOTOWY”, otwórz Docker Desktop → Troubleshoot → Restart. Potem użyj w launcherze „Diagnostyka” i „Zbierz logi (ZIP)”.' 'Wait a moment and try again. If the status does not change to "READY", open Docker Desktop > Troubleshoot > Restart. Then use DIAGNOSTICS and "Collect logs (ZIP)" in the launcher.')
        }
    }

    if ($value -match '(?i)cannot overwrite non-directory.+artifacts\.json.+with directory') {
        return [pscustomobject]@{
            Code = 'LEGACY_INSTALLER_DESTINATION'
            Title = (UI-Text 'Wybrany folder zawiera inną instalację' 'The chosen folder holds another installation')
            Message = (UI-Text 'Stary install.ps1 próbuje skopiować paczkę na istniejący plik lub do niezgodnego układu katalogów.' 'The old install.ps1 tries to copy the package over an existing file or into a folder layout that does not match.')
            Remedy = (UI-Text 'Nie uruchamiaj starego install.ps1 na folderze obecnej paczki All-in-One. Rozpakuj pełną paczkę do pustego folderu i uruchom Metin2-Launcher-GUI.bat. Istniejącej bazy Dockera nie usuwaj.' 'Do not run the old install.ps1 on the folder of the current All-in-One package. Unpack the full package into an empty folder and start Metin2-Launcher-GUI.bat. Do not delete the existing Docker database.')
        }
    }

    # Docker Desktop's own Linux disk went read-only or ran out of room, so the
    # image could not be written. Nothing of the server's files is touched;
    # the fix is free space and a clean restart of the WSL machine - and
    # Docker's "Clean / Purge data" only where there is no world yet, because
    # the database lives on that same disk. ext4 answers its first I/O error by
    # remounting itself read-only, so the first build says "input/output
    # error" about buildkit's own files, the next says "read-only file
    # system", and every one after that only "failed to solve: exit code:
    # 255" - which no pattern here can read, and which is why the preflight
    # now writes to that disk before anything is built (pattsito, 23
    # September: five updates in forty minutes, each ending there).
    if ($value -match '(?i)read-only file system|no space left on device|(?:/var/lib/(?:docker|desktop-containerd)|buildkit)[^\r\n]*input/output error|desktop-containerd.+meta\.db') {
        return [pscustomobject]@{
            Code = 'DOCKER_DISK_BROKEN'
            Title = (UI-Text 'Dysk maszyny Dockera jest tylko do odczytu albo pełny' 'The Docker machine''s disk is read-only or full')
            Message = (UI-Text 'Docker nie mógł zapisać na swoim dysku (plik docker_data.vhdx) - komunikat „read-only file system”, „input/output error” albo „no space left on device”. Zwykle zabrakło miejsca na dysku Windows, na którym leży ten plik, albo Docker Desktop zamknął się nieczysto. Pliki serwera są w porządku; baza świata leży na tym samym dysku Dockera.' 'Docker could not write to its disk (the file docker_data.vhdx) - the message "read-only file system", "input/output error" or "no space left on device". Usually the Windows drive holding that file ran out of room, or Docker Desktop closed uncleanly. The server files are fine; the world''s database is on the same Docker disk.')
            Remedy = (Get-M2DockerDiskRemedy)
        }
    }
    if ($value -match "(?i)playerbot-migrate.+didn.t complete successfully|database import was not ready after|user: 'unauthenticated'") {
        return [pscustomobject]@{
            Code = 'DB_USER_BROKEN'
            Title = (UI-Text 'Serwer nie może zalogować się do własnej bazy' 'The server cannot log in to its own database')
            Message = (UI-Text 'Baza działa, ale techniczne konto, którym łączy się serwer, nie jest rozpoznawane. Dlatego krok „playerbot-migrate” nie kończy się poprawnie, a gra i panel nie wstają. Twoje postacie, przedmioty i boty są bezpieczne.' 'The database runs, but the technical account the server connects with is not recognised. So the "playerbot-migrate" step does not finish properly, and the game and the panel do not come up. Your characters, items and bots are safe.')
            Remedy = (UI-Text 'W launcherze kliknij „NAPRAW DOSTĘP DO BAZY”, poczekaj na komunikat „Gotowe”, a potem kliknij „GRAJ”. Nie usuwaj wolumenów i nie używaj docker compose down -v.' 'In the launcher click REPAIR DATABASE ACCESS, wait for "Done", and then click PLAY. Do not delete the volumes and do not use docker compose down -v.')
        }
    }

    # apt inside a build refuses a Release file dated after the machine's
    # clock, and Docker Desktop's machine takes the Windows clock: Xewi's
    # Windows ran three hours behind (19 September), so every start stopped at
    # the panel's apt-get while the containers built earlier - "z dockera
    # dziala" - still ran.
    if ($value -match '(?i)is not valid yet \(invalid for another') {
        return [pscustomobject]@{
            Code = 'CLOCK_BEHIND'
            Title = (UI-Text 'Zegar komputera jest przestawiony' 'The computer''s clock is wrong')
            Message = (UI-Text 'Budowa serwera zatrzymała się, bo zegar Windows - a za nim Docker - jest opóźniony względem prawdziwego czasu i serwer pakietów odrzucił pobieranie (komunikat „Release file ... is not valid yet”). Pliki serwera i baza są w porządku.' 'The server build stopped because the Windows clock - and Docker''s with it - is behind the real time, and the package server refused the download (the message "Release file ... is not valid yet"). The server files and the database are fine.')
            Remedy = (UI-Text 'W Windows otwórz Ustawienia → Czas i język → Data i godzina, włącz „Ustaw czas automatycznie”, sprawdź strefę czasową (dla Polski: Warszawa) i kliknij „Synchronizuj teraz”. Potem zamknij Docker Desktop (ikona w zasobniku → Quit) i kliknij GRAJ.' 'In Windows open Settings > Time & language > Date & time, turn on "Set time automatically", check the time zone and click "Sync now". Then quit Docker Desktop (the tray icon > Quit) and click PLAY.')
        }
    }

    # An image the build makes itself, looked for on Docker Hub by an older
    # Compose: seban-collector and seban-item-grants run the image the
    # seban-panel service builds, and the compose file says pull_policy: never
    # for them only from 2.0.77 on.
    if ($value -match '(?i)pull access denied for metin2/') {
        return [pscustomobject]@{
            Code = 'LOCAL_IMAGE_PULLED'
            Title = (UI-Text 'Docker szukał w internecie obrazu, który serwer buduje sam' 'Docker looked on the Internet for an image the server builds itself')
            Message = (UI-Text 'Starszy Docker Compose próbował pobrać z Docker Hub obraz panelu zaawansowanego (metin2/seban-panel), zanim go zbudował, i przerwał start. Pliki serwera i baza są w porządku.' 'An older Docker Compose tried to pull the advanced panel''s image (metin2/seban-panel) from Docker Hub before building it, and stopped the start. The server files and the database are fine.')
            Remedy = (UI-Text 'Kliknij GRAJ jeszcze raz. Jeśli błąd wróci, otwórz PowerShell w folderze serwera, w podfolderze linux-port\docker, wykonaj: docker compose build seban-panel, a potem kliknij GRAJ. Logowanie do Docker Hub (docker login) niczego tu nie zmienia.' 'Click PLAY again. If the error comes back, open PowerShell in the server folder, in the subfolder linux-port\docker, run: docker compose build seban-panel, and then click PLAY. Logging in to Docker Hub (docker login) changes nothing here.')
        }
    }

    # Only a 404 said of the manifest itself. This reads the whole output of
    # the failed action, and a log carries "404" somewhere - the panel's answer
    # to a missing icon, a pid, a coordinate - so the bare number sent
    # archonek to wait for an update channel that was there all along
    # (18 September: the manifest answered 200, the start had failed).
    if ($value -match '(?i)update-manifest[^\r\n]*\b404\b|\b404\b[^\r\n]*update-manifest|not found[^\r\n]+update-manifest|update-manifest[^\r\n]+not found') {
        return [pscustomobject]@{
            Code = 'UPDATE_CHANNEL_UNPUBLISHED'
            Title = (UI-Text 'Kanał aktualizacji nie został jeszcze opublikowany' 'The update channel has not been published yet')
            Message = (UI-Text 'Serwer GitHub nie ma obecnie manifestu aktualizacji. Nie oznacza to uszkodzenia zainstalowanego serwera.' 'GitHub has no update manifest right now. That does not mean the installed server is damaged.')
            Remedy = (UI-Text 'Możesz nadal grać na obecnej wersji. Spróbuj ponownie później; launcher nie powinien niczego instalować ani tworzyć drugiego serwera.' 'You can go on playing the current version. Try again later; the launcher should install nothing and make no second server meanwhile.')
        }
    }

    return [pscustomobject]@{
        Code = 'UNKNOWN'
        Title = (UI-Text 'Operacja nie powiodła się' 'The operation failed')
        Message = if ($value) { ($value -split '\r?\n' | Select-Object -Last 1) } else { (UI-Text 'Nie otrzymano szczegółów błędu.' 'No details of the error came back.') }
        Remedy = (UI-Text 'Uruchom „Diagnostyka”, następnie „Zbierz logi (ZIP)” i prześlij utworzony plik na kanał pomocy projektu.' 'Run DIAGNOSTICS, then "Collect logs (ZIP)", and send the file it makes to the project''s help channel.')
    }
}

# What to do about a Docker disk that refuses writes: the guidance dialog, the
# preflight's blocking issue and the updater's refusal all say the same thing.
# One line per step, and never "wsl" followed on its line by "error",
# "failed" or "exit status": Get-M2LauncherErrorGuidance reads whole outputs,
# and a sentence of this remedy printed by the preflight must not look like a
# broken WSL to the rule above it.
function Get-M2DockerDiskRemedy {
    return ((UI-Text '1. Zwolnij miejsce na dysku z folderem Dockera (%LOCALAPPDATA%\Docker, zwykle C:) - budowa serwera potrzebuje ok. 15 GB.' '1. Free some room on the drive with Docker''s folder (%LOCALAPPDATA%\Docker, usually C:) - the server build needs about 15 GB.') + [Environment]::NewLine +
            (UI-Text '2. Zamknij Docker Desktop (ikona w zasobniku → Quit) i w PowerShell wpisz: wsl --shutdown' '2. Quit Docker Desktop (the tray icon > Quit) and type in PowerShell: wsl --shutdown') + [Environment]::NewLine +
            (UI-Text '3. Uruchom Docker Desktop, poczekaj na „Engine running” i kliknij GRAJ - launcher dokończy budowanie bez ponownego pobierania.' '3. Start Docker Desktop, wait for "Engine running" and click PLAY - the launcher finishes the build without downloading again.') + [Environment]::NewLine +
            (UI-Text 'Jeśli to nie pomoże, a masz już świat z postaciami, nie używaj w Docker Desktop „Clean / Purge data” ani „Reset to factory defaults” (kasują bazę) - dołącz logi do zgłoszenia na GitHubie. ' 'If that does not help and you already have a world with characters, do not use "Clean / Purge data" or "Reset to factory defaults" in Docker Desktop (they delete the database) - attach the logs to a report on GitHub. ') +
            (UI-Text 'Na świeżej instalacji, która jeszcze ani razu nie wystartowała, Docker Desktop → Troubleshoot → Clean / Purge data niczego nie zabierze i zakłada Dockerowi nowy dysk.' 'On a fresh install that has never started yet, Docker Desktop > Troubleshoot > Clean / Purge data takes nothing away and gives Docker a new disk.'))
}

function Format-M2Bytes {
    param([long]$Bytes)
    if ($Bytes -ge 1GB) { return ('{0:N1} GB' -f ($Bytes / 1GB)) }
    return ('{0:N0} MB' -f ($Bytes / 1MB))
}

# Where Docker Desktop keeps its Linux disk, and how much room the Windows
# drive under it has left. Images, the build cache and every volume - the
# world's database with them - live in one ext4 file system inside
# docker_data.vhdx, which grows as a build writes; when Windows cannot give it
# the room, ext4 takes the write error and remounts itself read-only. Moved
# with Docker Desktop's "Disk image location", the folder is named in its
# settings (customWslDistroDir; dataFolder for the Hyper-V backend).
function Get-M2DockerDataLocation {
    $directory = ''
    $appData = [Environment]::GetFolderPath('ApplicationData')
    if ($appData) {
        foreach ($name in @('settings-store.json', 'settings.json')) {
            $path = Join-Path (Join-Path $appData 'Docker') $name
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
            try {
                $settings = Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
                foreach ($property in @($settings.PSObject.Properties)) {
                    if ($property.Name -match '(?i)^(?:customWslDistroDir|dataFolder)$' -and [string]$property.Value) {
                        $directory = [string]$property.Value
                        break
                    }
                }
            }
            catch {}
            if ($directory) { break }
        }
    }
    $localAppData = [Environment]::GetFolderPath('LocalApplicationData')
    if (-not $directory -and $localAppData) { $directory = Join-Path (Join-Path $localAppData 'Docker') 'wsl' }
    if (-not $directory) { return $null }

    $disks = @()
    if (Test-Path -LiteralPath $directory -PathType Container) {
        $disks = @(Get-ChildItem -LiteralPath $directory -Filter '*.vhdx' -Recurse -Depth 2 -File -ErrorAction SilentlyContinue |
            ForEach-Object { [pscustomobject]@{ Path = $_.FullName; Bytes = [long]$_.Length } })
    }
    $free = $null
    $total = $null
    $driveName = ''
    try {
        $rootPath = [IO.Path]::GetPathRoot([IO.Path]::GetFullPath($directory))
        if ($rootPath) {
            $drive = [IO.DriveInfo]::new($rootPath)
            $driveName = $drive.Name.TrimEnd('\')
            if ($drive.IsReady) {
                $free = [long]$drive.AvailableFreeSpace
                $total = [long]$drive.TotalSize
            }
        }
    }
    catch {}
    return [pscustomobject]@{
        Directory = $directory
        Drive = $driveName
        FreeBytes = $free
        TotalBytes = $total
        Disks = @($disks)
    }
}

# A write to Docker's disk, and nothing else. The engine goes on answering
# `docker info' after its disk has gone read-only, so the preflight said
# "mozna uruchomic serwer" while every build died at its first write. A volume
# created and removed at once is a write there (its directory and the volume
# store's database) that touches nothing of the stack. Answers the daemon's
# own words when the write fails for want of a disk, '' otherwise - including
# when it fails for any other reason, which is not this check's to name.
function Get-M2DockerDiskFault {
    $name = 'm2-disk-probe-' + [Guid]::NewGuid().ToString('N').Substring(0, 12)
    $create = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments ('volume create --label com.metin2.probe=1 ' + $name) -TimeoutMilliseconds 15000
    if ($create.ExitCode -eq 0) {
        [void](Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments ('volume rm -f ' + $name) -TimeoutMilliseconds 15000)
        return ''
    }
    $text = ([string]$create.Output).Trim()
    if ($text -notmatch '(?i)read-only file system|input/output error|no space left on device') { return '' }
    $lines = @($text -split '\r?\n' | Where-Object { $_.Trim() })
    return ([string]$lines[$lines.Count - 1]).Trim()
}

# What a support bundle says about disks, because the one question a report
# like pattsito's cannot answer without it is whether the drive was full.
# User profile paths are shortened to %USERPROFILE%.
function Get-M2DiskSpaceReport {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $profilePath = [Environment]::GetFolderPath('UserProfile')
    $hide = {
        param([string]$Path)
        if ($profilePath -and $Path.StartsWith($profilePath, [StringComparison]::OrdinalIgnoreCase)) {
            return '%USERPROFILE%' + $Path.Substring($profilePath.Length)
        }
        return $Path
    }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add((UI-Text 'Dyski Windows:' 'Windows drives:'))
    foreach ($drive in @([IO.DriveInfo]::GetDrives())) {
        try {
            if ($drive.DriveType -ne [IO.DriveType]::Fixed -or -not $drive.IsReady) { continue }
            $lines.Add(((UI-Text '  {0} wolne {1} z {2}' '  {0} free {1} of {2}') -f $drive.Name.TrimEnd('\'), (Format-M2Bytes $drive.AvailableFreeSpace), (Format-M2Bytes $drive.TotalSize)))
        }
        catch {}
    }
    $lines.Add('')
    $lines.Add('Docker Desktop:')
    $data = Get-M2DockerDataLocation
    if ($data) {
        $lines.Add((UI-Text '  folder dysku: ' '  disk folder: ') + (& $hide $data.Directory))
        if (@($data.Disks).Count -eq 0) { $lines.Add((UI-Text '  (nie znaleziono plikow .vhdx)' '  (no .vhdx files found)')) }
        foreach ($disk in @($data.Disks)) {
            $lines.Add(('  {0}: {1}' -f (& $hide $disk.Path), (Format-M2Bytes $disk.Bytes)))
        }
        if ($null -ne $data.FreeBytes) {
            $lines.Add(((UI-Text '  wolne na dysku {0} {1}' '  free on the drive {0} {1}') -f $data.Drive, (Format-M2Bytes $data.FreeBytes)))
        }
    }
    else {
        $lines.Add((UI-Text '  (nie ustalono folderu dysku)' '  (the disk folder was not found)'))
    }
    $lines.Add('')
    $lines.Add((UI-Text 'Kopie aktualizacji (backups):' 'Update backups (backups):'))
    $backups = Join-Path ([IO.Path]::GetFullPath($ServerRoot)) 'backups'
    if (Test-Path -LiteralPath $backups -PathType Container) {
        $copies = @(Get-ChildItem -LiteralPath $backups -Directory -ErrorAction SilentlyContinue)
        $bytes = 0L
        foreach ($file in @(Get-ChildItem -LiteralPath $backups -Recurse -File -Force -ErrorAction SilentlyContinue)) { $bytes += [long]$file.Length }
        $lines.Add(((UI-Text '  {0} katalogow, razem {1}' '  {0} folders, {1} in all') -f $copies.Count, (Format-M2Bytes $bytes)))
    }
    else {
        $lines.Add((UI-Text '  (brak)' '  (none)'))
    }
    return ($lines -join [Environment]::NewLine)
}

function Get-M2InstallationProjectName {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $statePath = Join-Path $ServerRoot '.m2install.json'
    if (Test-Path -LiteralPath $statePath -PathType Leaf) {
        try {
            $state = Get-Content -LiteralPath $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
            if ([string]$state.projectName) { return [string]$state.projectName }
        }
        catch {}
    }

    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_COMPOSE_PROJECT_NAME=(.+)$' | Select-Object -First 1
        if ($match -and $match.Matches[0].Groups[1].Value) {
            return $match.Matches[0].Groups[1].Value.Trim()
        }
    }
    return ''
}

function Get-M2PanelPort {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        $match = Select-String -LiteralPath $envPath -Pattern '^M2_PANEL_PUBLIC_PORT=(\d+)$' | Select-Object -First 1
        if ($match) { return [int]$match.Matches[0].Groups[1].Value }
    }
    return 7788
}

function Get-M2ListeningProcess {
    param([Parameter(Mandatory = $true)][int]$Port)

    try {
        $connection = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction Stop | Select-Object -First 1
        if (-not $connection) { return $null }
        $processName = ''
        try { $processName = (Get-Process -Id $connection.OwningProcess -ErrorAction Stop).ProcessName } catch {}
        return [pscustomobject]@{ Pid = [int]$connection.OwningProcess; Name = $processName }
    }
    catch {
        $netstat = Invoke-M2DiagnosticProcess -FileName 'netstat.exe' -Arguments '-ano -p tcp' -TimeoutMilliseconds 3000
        if ($netstat.ExitCode -ne 0) { return $null }
        foreach ($line in ($netstat.Output -split '\r?\n')) {
            if ($line -match ('(?i)^\s*TCP\s+\S+:' + $Port + '\s+\S+\s+LISTENING\s+(\d+)\s*$')) {
                $pidValue = [int]$Matches[1]
                $processName = ''
                try { $processName = (Get-Process -Id $pidValue -ErrorAction Stop).ProcessName } catch {}
                return [pscustomobject]@{ Pid = $pidValue; Name = $processName }
            }
        }
    }
    return $null
}

function Get-M2DockerPortOwner {
    param(
        [Parameter(Mandatory = $true)][int]$Port,
        [AllowEmptyString()][string]$CurrentProject = ''
    )

    foreach ($holder in @(Get-M2DockerPortHolders -Ports @($Port) -CurrentProject $CurrentProject)) {
        return $holder
    }
    return $null
}

# Which of the wanted ports a container's PORTS column actually publishes.
#
# The column is a comma-separated list of "[host:]HOST->CONTAINER/proto", and a
# published range collapses into a single entry: "127.0.0.1:13000-13002->
# 13000-13002/tcp". The host side is what matters, and it is either one number
# or two around a dash.
function Get-M2PublishedPortMatches {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$PortsText,
        [Parameter(Mandatory = $true)][int[]]$Ports
    )

    $matched = @()
    if (-not $PortsText) { return $matched }
    foreach ($entry in ($PortsText -split ',')) {
        $piece = $entry.Trim()
        if (-not $piece -or $piece -notmatch '->') { continue }
        $hostSide = ($piece -split '->')[0].Trim()
        # Drop the bind address; IPv6 arrives as "[::]:7788".
        if ($hostSide -match '^\[[^\]]*\]:(.+)$') { $hostSide = $Matches[1] }
        elseif ($hostSide -match '^[^:]*:(.+)$') { $hostSide = $Matches[1] }
        $first = 0
        $last = 0
        if ($hostSide -match '^(\d+)-(\d+)$') { $first = [int]$Matches[1]; $last = [int]$Matches[2] }
        elseif ($hostSide -match '^(\d+)$') { $first = [int]$Matches[1]; $last = $first }
        else { continue }
        foreach ($port in @($Ports)) {
            $number = [int]$port
            if ($number -ge $first -and $number -le $last -and $matched -notcontains $number) {
                $matched += $number
            }
        }
    }
    return $matched
}

# Every running container that publishes one of these host ports, with its
# compose project and the folder it was started from. The folder is the half an
# operator needs and never had: one machine here carries five projects of this
# same server (m2dep, m2zip, m2mt, m2fresh, metin2), each from its own
# directory, and "another installation is using the port" without naming which
# one is advice nobody can act on.
function Get-M2DockerPortHolders {
    param(
        [Parameter(Mandatory = $true)][int[]]$Ports,
        [AllowEmptyString()][string]$CurrentProject = ''
    )

    $holders = @()
    if (@($Ports).Count -eq 0) { return $holders }
    $dockerPs = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments 'ps --format "{{json .}}"' -TimeoutMilliseconds 6000
    if ($dockerPs.ExitCode -ne 0) { return $holders }
    foreach ($line in ($dockerPs.Output -split '\r?\n')) {
        if (-not $line.Trim()) { continue }
        try { $container = $line | ConvertFrom-Json } catch { continue }
        # Docker prints a published range as one entry - "127.0.0.1:13000-13002
        # ->13000-13002/tcp" - so matching the literal "13001->" found nothing
        # and the three game channels looked like they belonged to no container
        # at all. The preflight then called the player's own running server a
        # foreign program and refused to start (sizowski, 13 September, on the
        # very check meant to help). Every host side is parsed, single or range.
        $matched = @(Get-M2PublishedPortMatches -PortsText ([string]$container.Ports) -Ports $Ports)
        if ($matched.Count -eq 0) { continue }
        $project = ''
        $workingDir = ''
        $labels = [string]$container.Labels
        if ($labels -match '(?:^|,)com\.docker\.compose\.project=([^,]+)') { $project = $Matches[1] }
        if ($labels -match '(?:^|,)com\.docker\.compose\.project\.working_dir=([^,]+)') { $workingDir = $Matches[1] }
        $holders += [pscustomobject]@{
            Container = [string]$container.Names
            Project = $project
            WorkingDir = $workingDir
            Ports = @($matched)
            IsCurrentProject = [bool]($CurrentProject -and $project -and $project.Equals($CurrentProject, [StringComparison]::OrdinalIgnoreCase))
        }
    }
    return $holders
}

# The host ports this installation publishes, read from its own .env. The
# preflight used to look at the panel's 7788 alone, so a collision on 7790 - the
# advanced panel, which a second copy of the server publishes too - passed the
# check with "OK: port panelu jest wolny" and then stopped compose after the
# images were built: "Bind for 127.0.0.1:7790 failed: port is already
# allocated". compose gives up on the first taken port, so the check has to know
# all of them.
function Get-M2StackHostPorts {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $envPath = Join-Path $ServerRoot 'linux-port\docker\.env'
    $values = @{}
    if (Test-Path -LiteralPath $envPath -PathType Leaf) {
        foreach ($line in @([IO.File]::ReadAllLines($envPath))) {
            if ("$line" -match '^\s*([A-Za-z0-9_]+)=(.*)$') { $values[$Matches[1]] = $Matches[2].Trim() }
        }
    }
    # ClientFixed: a port the game client dials by its own number. The local
    # server entry in the client's serverinfo.py logs in on 11000, and the
    # login answer names the core's own port inside the container, 13000 and
    # up - so moving either in .env starts a server nobody can log in to.
    # Kordix (23 September) was told by this check to do exactly that when
    # MSI_GamebarTool held 11000. Only the panels, the ItemShop and the
    # database are reached through the port .env publishes.
    $ports = @()
    foreach ($entry in @(
            @{ Key = 'M2_PANEL_PUBLIC_PORT'; Default = 7788; Name = (UI-Text 'panel WWW' 'web panel'); ClientFixed = $false },
            @{ Key = 'M2_SEBAN_PANEL_PORT'; Default = 7790; Name = (UI-Text 'panel zaawansowany' 'advanced panel'); ClientFixed = $false },
            @{ Key = 'M2_ITEMSHOP_PUBLIC_PORT'; Default = 7791; Name = 'ItemShop'; ClientFixed = $false },
            @{ Key = 'M2_AUTH_PORT'; Default = 11000; Name = (UI-Text 'serwer logowania' 'login server'); ClientFixed = $true },
            @{ Key = 'M2_DB_PUBLISH_PORT'; Default = 3306; Name = (UI-Text 'baza danych' 'database'); ClientFixed = $false })) {
        $value = [string]$values[$entry.Key]
        $number = [int]$entry.Default
        if ($value -match '^\d+$') { $number = [int]$value }
        # Key: the .env line that moves the port, which the advice names.
        $ports += [pscustomobject]@{ Port = $number; Name = [string]$entry.Name; ClientFixed = [bool]$entry.ClientFixed;
            Key = [string]$entry.Key }
    }
    # "13000-13002": each channel binds its own port and any one of them can be
    # the one that is taken.
    $first = 13000
    $last = 13002
    $range = [string]$values['M2_GAME_PORT_RANGE']
    if ($range -match '^(\d+)\s*-\s*(\d+)$') { $first = [int]$Matches[1]; $last = [int]$Matches[2] }
    elseif ($range -match '^(\d+)$') { $first = [int]$Matches[1]; $last = $first }
    if ($last -lt $first) { $last = $first }
    if (($last - $first) -gt 32) { $last = $first + 32 }
    for ($p = $first; $p -le $last; $p++) {
        $ports += [pscustomobject]@{ Port = [int]$p; Name = (UI-Text 'kanal gry' 'game channel'); ClientFixed = $true; Key = 'M2_GAME_PORT_RANGE' }
    }
    return $ports
}

# Containers of ANOTHER compose project sitting on this installation's ports.
# This is the recurring half of "port jest juz zajety" on a machine that has
# ever held a second copy of the server: every container ships with
# restart: unless-stopped, so Docker Desktop brings the old project back up on
# every engine start and it takes the ports before this installation can - which
# is exactly why quitting Docker by hand does not help. `docker stop` is the fix
# because that flag survives a restart; removing a volume never is.
function Get-M2ForeignPortHolders {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $project = Get-M2InstallationProjectName -ServerRoot $ServerRoot
    $ports = @(Get-M2StackHostPorts -ServerRoot $ServerRoot | ForEach-Object { [int]$_.Port })
    $holders = @(Get-M2DockerPortHolders -Ports $ports -CurrentProject $project)
    return @($holders | Where-Object { $_.Project -and -not $_.IsCurrentProject })
}

# What to do about a program of Windows' own on one of the server's ports: the
# start's preflight says it, and so does the update's check below, in one
# wording. A port the game client dials by its own number cannot move.
function Get-M2ProgramPortAdvice {
    param(
        [Parameter(Mandatory = $true)][int]$Port,
        [AllowEmptyString()][string]$Name = '',
        [bool]$ClientFixed = $false,
        [AllowEmptyString()][string]$Key = '',
        [Parameter(Mandatory = $true)]$Listener
    )

    $who = if ($Listener.Name) { (UI-Text "proces $($Listener.Name), PID $($Listener.Pid)" "the process $($Listener.Name), PID $($Listener.Pid)") } else { "PID $($Listener.Pid)" }
    # An ssh on a panel port is as a rule the VPS window's tunnel, and moving
    # the server's port is the wrong answer to it (Sudak, 28 September).
    if ([string]$Listener.Name -match '^(?i)ssh$') {
        return (UI-Text "Port $Port ($Name) zajmuje tunel SSH ($who). Jeśli to tunel do paneli z okna SERWER NA VPS, kliknij tam ZAMKNIJ TUNEL; inny tunel zamknij w Menedżerze zadań (karta Szczegóły, Zakończ zadanie). Potem spróbuj jeszcze raz." "Port $Port ($Name) is held by an SSH tunnel ($who). If it is the tunnel to the panels from the SERVER ON A VPS window, click CLOSE TUNNEL there; close any other tunnel in Task Manager (the Details tab, End task). Then try again.")
    }
    if ($ClientFixed -or -not $Key) {
        return (UI-Text "Port $Port ($Name) zajmuje $who. Zamknij ten program (Menedżer zadań, karta Szczegóły, Zakończ zadanie) i spróbuj jeszcze raz. Nie zmieniaj tego portu w pliku .env: klient gry łączy się zawsze z portem 11000 i kanałami od 13000, więc po zmianie nie dałoby się zalogować." "Port $Port ($Name) is held by $who. Close that program (Task Manager, the Details tab, End task) and try again. Do not change this port in the .env file: the game client always connects to port 11000 and to the channels from 13000 on, so after a change nobody could log in.")
    }
    return (UI-Text "Port $Port ($Name) zajmuje $who. Zamknij ten program (jeśli to usługa, np. MySQL, zatrzymaj ją w Usługach Windows) albo zmień w pliku linux-port\docker\.env wiersz $Key=$Port na inny wolny port, np. $($Port + 1)." "Port $Port ($Name) is held by $who. Close that program (if it is a service, e.g. MySQL, stop it in Windows Services) or change the line $Key=$Port in the file linux-port\docker\.env to another free port, e.g. $($Port + 1).")
}

# The server's ports a program of Windows' own listens on. Not Docker: its
# listeners stand for containers - this installation's, or another's, which
# Stop-M2ForeignPortHolders stops before a build - and a container publishing
# the port is Docker's however Windows names the listener. The update asks this
# before it downloads anything, because its build takes minutes and compose
# found such a port only at the very end (Producent Hip Hopu, 27 September: a
# MySQL on 3306, and the update's advice named the panel's 7788).
function Get-M2ProgramPortConflicts {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $busy = @()
    foreach ($entry in @(Get-M2StackHostPorts -ServerRoot $ServerRoot)) {
        $listener = Get-M2ListeningProcess -Port ([int]$entry.Port)
        if ($null -eq $listener) { continue }
        if ([string]$listener.Name -match '(?i)^(com\.docker|docker|vpnkit|wslrelay)') { continue }
        $busy += [pscustomobject]@{
            Port = [int]$entry.Port
            Name = [string]$entry.Name
            ClientFixed = [bool]$entry.ClientFixed
            Key = [string]$entry.Key
            Listener = $listener
        }
    }
    if ($busy.Count -eq 0) { return @() }
    $published = @()
    foreach ($holder in @(Get-M2DockerPortHolders -Ports @($busy | ForEach-Object { [int]$_.Port }))) {
        $published += @($holder.Ports | ForEach-Object { [int]$_ })
    }
    $conflicts = @()
    foreach ($entry in $busy) {
        if ($published -contains [int]$entry.Port) { continue }
        $conflicts += [pscustomobject]@{
            Port = $entry.Port
            Name = $entry.Name
            ClientFixed = $entry.ClientFixed
            Key = $entry.Key
            Listener = $entry.Listener
            Advice = (Get-M2ProgramPortAdvice -Port $entry.Port -Name $entry.Name -ClientFixed $entry.ClientFixed -Key $entry.Key -Listener $entry.Listener)
        }
    }
    return $conflicts
}

function Stop-M2ForeignPortHolders {
    param([Parameter(Mandatory = $true)][string]$ServerRoot)

    $stopped = @()
    $seen = @{}
    foreach ($holder in @(Get-M2ForeignPortHolders -ServerRoot $ServerRoot)) {
        if ($seen.ContainsKey($holder.Project)) { continue }
        $seen[$holder.Project] = $true
        # The whole project, not just the container that answered: its siblings
        # hold the remaining ports and the same restart policy would bring them
        # back on the next engine start.
        $ids = @(& docker ps -aq --filter ("label=com.docker.compose.project=" + $holder.Project) 2>$null |
            Where-Object { $_ })
        if ($ids.Count -eq 0) { continue }
        & docker stop $ids 1>$null 2>$null
        $stopped += [pscustomobject]@{
            Project = [string]$holder.Project
            WorkingDir = [string]$holder.WorkingDir
            Containers = $ids.Count
            Ports = @($holder.Ports)
        }
    }
    return $stopped
}

# The TCP ranges Windows has reserved for itself (Hyper-V, WSL, NAT). A port
# inside one cannot be bound by anything, Docker included, and the failure
# only shows up a second after the images are built. Empty when netsh is
# missing or says nothing - never a reason to refuse a start on its own.
function Get-M2ExcludedPortRanges {
    $ranges = @()
    try {
        $lines = & netsh interface ipv4 show excludedportrange protocol=tcp 2>$null
        foreach ($line in @($lines)) {
            if ("$line" -match '^\s*(\d+)\s+(\d+)\s*\*?\s*$') {
                $ranges += [pscustomobject]@{ Start = [int]$Matches[1]; End = [int]$Matches[2] }
            }
        }
    }
    catch { }
    return $ranges
}

function Get-M2ExcludedPortHit {
    param([Parameter(Mandatory = $true)][int]$Port, [object[]]$Ranges)
    foreach ($r in @($Ranges)) {
        if ($Port -ge $r.Start -and $Port -le $r.End) { return $r }
    }
    return $null
}

function Get-DockerDesktopCandidates {
    # The three stock folders, then wherever the CLI on PATH lives (Docker
    # Desktop keeps docker.exe under <install>\resources\bin), then the
    # uninstall entry's InstallLocation. Two players had Docker on another
    # drive: the launcher stopped Docker Desktop for them and then could not
    # start it again, and the update that happened to come between the two
    # got the blame.
    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($p in @(
            (Join-Path $env:ProgramFiles 'Docker\Docker\Docker Desktop.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'Docker\Docker\Docker Desktop.exe'),
            (Join-Path $env:LOCALAPPDATA 'Docker\Docker Desktop.exe'))) {
        if ($p) { $paths.Add($p) }
    }
    try {
        $cli = (Get-Command docker -ErrorAction Stop).Source
        if ($cli) {
            $root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $cli))
            if ($root) { $paths.Add((Join-Path $root 'Docker Desktop.exe')) }
        }
    }
    catch { }
    foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop',
                       'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop')) {
        try {
            $loc = (Get-ItemProperty -LiteralPath $key -ErrorAction Stop).InstallLocation
            if ($loc) { $paths.Add((Join-Path $loc 'Docker Desktop.exe')) }
        }
        catch { }
    }
    return @($paths | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -Unique)
}

function Get-M2ClockSkewText {
    param([int]$Seconds)
    $s = [Math]::Abs($Seconds)
    if ($s -ge 3600) { return ('{0} h {1:D2} min' -f [int][Math]::Floor($s / 3600), [int][Math]::Floor(($s % 3600) / 60)) }
    if ($s -ge 60) { return ('{0} min' -f [int][Math]::Floor($s / 60)) }
    return ('{0} s' -f $s)
}

function Get-M2InternetClockSkew {
    # Seconds the Windows clock runs ahead (+) or behind (-) of the time an
    # HTTPS server puts in its Date header; $null when none answered. Docker
    # Desktop's machine takes the Windows clock, and apt inside a build refuses
    # a Release file dated after it ("not valid yet"): Xewi's Windows ran three
    # hours behind on 19 September and no build got past the panel's apt-get.
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12 } catch { }
    $response = $null
    $date = ''
    $answeredAt = [DateTime]::UtcNow
    try {
        $request = [Net.HttpWebRequest]::Create('https://api.github.com/')
        $request.Method = 'HEAD'
        $request.Timeout = 4000
        $request.UserAgent = 'metin2-playerbots-launcher'
        try { $response = $request.GetResponse() }
        catch {
            # An HTTP error still carries the header. PowerShell hands the
            # WebException over wrapped, so walk down to it.
            $inner = $_.Exception
            while ($inner -and -not ($inner -is [Net.WebException])) { $inner = $inner.InnerException }
            if ($inner) { $response = $inner.Response }
        }
        $answeredAt = [DateTime]::UtcNow
        if ($response) { $date = [string]$response.Headers['Date'] }
    }
    catch { return $null }
    finally { if ($response) { $response.Close() } }
    if (-not $date) { return $null }
    $parsed = [DateTime]::MinValue
    $styles = [Globalization.DateTimeStyles]::AdjustToUniversal -bor [Globalization.DateTimeStyles]::AssumeUniversal
    if (-not [DateTime]::TryParse($date, [Globalization.CultureInfo]::InvariantCulture, $styles, [ref]$parsed)) { return $null }
    return [int][Math]::Round(($answeredAt - $parsed).TotalSeconds)
}

function Get-M2DockerClockSkew {
    # Seconds Docker's machine runs ahead (+) or behind (-) of Windows. It is
    # set from Windows when it starts and can fall behind after the computer
    # sleeps; a restart of Docker Desktop puts it right.
    $probe = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments 'info --format "{{.SystemTime}}"' -TimeoutMilliseconds 3500
    if ($probe.ExitCode -ne 0 -or $probe.TimedOut) { return $null }
    if ([string]$probe.Output -notmatch '(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d)(?:\.\d+)?(Z|[+-]\d\d:\d\d)') { return $null }
    $stamp = $Matches[1] + $(if ($Matches[2] -eq 'Z') { '+00:00' } else { $Matches[2] })
    $parsed = [DateTimeOffset]::MinValue
    if (-not [DateTimeOffset]::TryParseExact($stamp, "yyyy-MM-dd'T'HH:mm:sszzz", [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::None, [ref]$parsed)) { return $null }
    return [int][Math]::Round(($parsed.UtcDateTime - [DateTime]::UtcNow).TotalSeconds)
}

function Get-M2DockerPreflight {
    param(
        [Parameter(Mandatory = $true)][string]$ServerRoot,
        [switch]$CheckPanelPort
    )

    $root = [IO.Path]::GetFullPath($ServerRoot)
    $checks = [Collections.ArrayList]::new()
    $blocking = [Collections.ArrayList]::new()
    $warnings = [Collections.ArrayList]::new()
    $dockerCommand = Get-Command docker -ErrorAction SilentlyContinue
    $dockerCliPresent = $null -ne $dockerCommand
    $dockerProcessesRunning = @(Get-Process -Name 'Docker Desktop', 'com.docker.backend' -ErrorAction SilentlyContinue).Count -gt 0
    $dockerEngineReady = $false

    if ($dockerCliPresent) {
        [void]$checks.Add((UI-Text 'OK: Docker CLI jest zainstalowany.' 'OK: the Docker CLI is installed.'))
        $dockerProbe = Invoke-M2DiagnosticProcess -FileName 'docker.exe' -Arguments 'info --format "{{.ServerVersion}}"' -TimeoutMilliseconds 3500
        $dockerProbeText = if ($null -ne $dockerProbe.Output) { $dockerProbe.Output.Trim() } else { '' }
        # `docker info` exits 0 and still prints the daemon's own refusal. "Error
        # response from daemon: Docker Desktop is unable to start" arrives on
        # stderr, where the server version should be, and the exit code says
        # nothing is wrong - so the check reported "OK: Docker Engine odpowiada
        # (wersja Error response from daemon: Docker Desktop is unable to start)"
        # and a verdict of "mozna uruchomic serwer", six times over, to a player
        # whose WSL was broken. He therefore never saw the one warning that names
        # what to repair, because that warning is only raised when the engine is
        # known to be down. A version is digits and dots; anything else is the
        # engine failing to answer.
        $dockerVersion = ''
        foreach ($probeLine in ($dockerProbeText -split "`r?`n")) {
            $candidate = $probeLine.Trim()
            if ($candidate -match '^\d+(\.\d+)+') {
                $dockerVersion = $candidate
                break
            }
        }
        $dockerEngineReady = $dockerProbe.ExitCode -eq 0 -and -not $dockerProbe.TimedOut -and $dockerVersion -ne ''
        if ($dockerEngineReady) {
            [void]$checks.Add((UI-Text "OK: Docker Engine odpowiada (wersja $dockerVersion)." "OK: Docker Engine answers (version $dockerVersion)."))
        }
        elseif ($dockerProbeText -and -not $dockerProbe.TimedOut) {
            # What the daemon said, not a tidy summary of it: the message names
            # the fault and the player pastes it straight into a report.
            [void]$checks.Add((UI-Text "BLAD: Docker Engine nie odpowiada: $dockerProbeText" "ERROR: Docker Engine does not answer: $dockerProbeText"))
            [void]$warnings.Add((UI-Text "Silnik Dockera nie wystartowal: $dockerProbeText" "The Docker engine did not start: $dockerProbeText"))
        }
        elseif ($dockerProcessesRunning) {
            [void]$checks.Add((UI-Text 'UWAGA: Docker Desktop jest otwarty, ale Engine jeszcze nie odpowiada.' 'WARNING: Docker Desktop is open, but the Engine does not answer yet.'))
            [void]$warnings.Add((UI-Text 'Docker Desktop nadal startuje albo zatrzymał się na błędzie.' 'Docker Desktop is still starting or has stopped on an error.'))
        }
        else {
            [void]$checks.Add((UI-Text 'INFO: Docker Engine jest zatrzymany; launcher może go uruchomić.' 'INFO: Docker Engine is stopped; the launcher can start it.'))
        }
    }
    else {
        [void]$checks.Add((UI-Text 'BŁĄD: nie znaleziono Docker CLI.' 'ERROR: the Docker CLI was not found.'))
        [void]$blocking.Add((UI-Text 'Zainstaluj Docker Desktop z oficjalnej strony i uruchom ponownie launcher.' 'Install Docker Desktop from its official website and start the launcher again.'))
    }

    $desktopCandidates = @(Get-DockerDesktopCandidates)
    if (-not $dockerEngineReady -and $desktopCandidates.Count -eq 0) {
        [void]$blocking.Add((UI-Text 'Nie znaleziono programu Docker Desktop. Zainstaluj go przed uruchomieniem serwera.' 'Docker Desktop was not found. Install it before starting the server.'))
    }

    $virtualization = 'Unknown'
    try {
        $computer = Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop
        $processor = Get-CimInstance -ClassName Win32_Processor -ErrorAction Stop | Select-Object -First 1
        if ([bool]$computer.HypervisorPresent -or [bool]$processor.VirtualizationFirmwareEnabled) {
            $virtualization = 'Enabled'
            [void]$checks.Add((UI-Text 'OK: wirtualizacja procesora jest dostępna.' 'OK: the processor''s virtualization is available.'))
        }
        elseif ($null -ne $processor.VirtualizationFirmwareEnabled) {
            $virtualization = 'Disabled'
            [void]$checks.Add((UI-Text 'BŁĄD: wirtualizacja procesora jest wyłączona w BIOS/UEFI.' 'ERROR: the processor''s virtualization is off in the BIOS/UEFI.'))
            if (-not $dockerEngineReady) {
                [void]$blocking.Add((UI-Text 'Włącz AMD SVM/AMD-V albo Intel VT-x w BIOS/UEFI, a następnie zrestartuj komputer.' 'Turn on AMD SVM/AMD-V or Intel VT-x in the BIOS/UEFI, then restart the computer.'))
            }
        }
    }
    catch {
        [void]$checks.Add((UI-Text 'INFO: Windows nie udostępnił stanu wirtualizacji; Docker zweryfikuje go przy starcie.' 'INFO: Windows did not tell the state of virtualization; Docker checks it when it starts.'))
    }

    $wslState = 'Missing'
    $wslCommand = Get-Command wsl.exe -ErrorAction SilentlyContinue
    if ($wslCommand) {
        $wslProbe = Invoke-M2DiagnosticProcess -FileName 'wsl.exe' -Arguments '--status' -TimeoutMilliseconds 4500
        if ($wslProbe.ExitCode -eq 0) {
            $wslState = 'Ready'
            [void]$checks.Add((UI-Text 'OK: WSL odpowiada.' 'OK: WSL answers.'))
        }
        else {
            $wslState = 'Error'
            [void]$checks.Add((UI-Text 'UWAGA: polecenie wsl --status nie działa poprawnie.' 'WARNING: the command wsl --status does not work properly.'))
            if (-not $dockerEngineReady) {
                [void]$warnings.Add((UI-Text 'Jeżeli Docker nie wystartuje, uruchom PowerShell jako administrator, wykonaj wsl --update i wsl --install, a następnie zrestartuj Windows.' 'If Docker does not start, run PowerShell as administrator, run wsl --update and wsl --install, then restart Windows.'))
            }
        }
    }
    else {
        [void]$checks.Add((UI-Text 'UWAGA: Windows Subsystem for Linux nie jest zainstalowany lub nie jest widoczny.' 'WARNING: Windows Subsystem for Linux is not installed or cannot be seen.'))
        if (-not $dockerEngineReady) {
            [void]$warnings.Add((UI-Text 'Docker Desktop zwykle wymaga WSL 2. W razie błędu wykonaj jako administrator: wsl --install, a potem zrestartuj Windows.' 'Docker Desktop usually needs WSL 2. If Docker does not start, run as administrator: wsl --install, then restart Windows.'))
        }
    }

    $panelPort = Get-M2PanelPort -ServerRoot $root
    $portOwner = $null
    $dockerPortOwner = $null
    $currentProject = Get-M2InstallationProjectName -ServerRoot $root
    $foreignHolders = @()
    if ($CheckPanelPort) {
        $busy = @()
        foreach ($entry in @(Get-M2StackHostPorts -ServerRoot $root)) {
            $listener = Get-M2ListeningProcess -Port ([int]$entry.Port)
            if ($null -ne $listener) {
                $busy += [pscustomobject]@{
                    Port = [int]$entry.Port
                    Name = [string]$entry.Name
                    ClientFixed = [bool]$entry.ClientFixed
                    Key = [string]$entry.Key
                    Listener = $listener
                }
            }
        }
        $holders = @()
        if ($busy.Count -gt 0 -and $dockerEngineReady) {
            $holders = @(Get-M2DockerPortHolders -Ports @($busy | ForEach-Object { [int]$_.Port }) -CurrentProject $currentProject)
        }
        if ($busy.Count -eq 0) {
            [void]$checks.Add((UI-Text 'OK: wszystkie porty serwera są wolne.' 'OK: every server port is free.'))
        }
        foreach ($entry in $busy) {
            $holder = @($holders | Where-Object { $_.Ports -contains [int]$entry.Port }) | Select-Object -First 1
            if ($holder -and $holder.IsCurrentProject) {
                [void]$checks.Add((UI-Text "OK: port $($entry.Port) ($($entry.Name)) należy do tej instalacji ($($holder.Container))." "OK: port $($entry.Port) ($($entry.Name)) belongs to this installation ($($holder.Container))."))
            }
            elseif ($holder) {
                $where = if ($holder.WorkingDir) { ", folder $($holder.WorkingDir)" } else { '' }
                [void]$checks.Add((UI-Text "BŁĄD: port $($entry.Port) ($($entry.Name)) zajmuje kontener $($holder.Container) z innej instalacji (projekt $($holder.Project)$where)." "ERROR: port $($entry.Port) ($($entry.Name)) is held by the container $($holder.Container) of another installation (project $($holder.Project)$where)."))
                $foreignHolders += $holder
            }
            elseif ($entry.Listener.Name -match '(?i)^(com\.docker|docker|vpnkit|wslrelay)') {
                # Docker itself holds every published port on Windows, in the
                # name of some container. Not recognising which one is a gap in
                # this check, never a reason to refuse the start: saying "close
                # com.docker.backend" to somebody whose own server is running is
                # advice that cannot be followed.
                [void]$checks.Add((UI-Text "UWAGA: port $($entry.Port) ($($entry.Name)) trzyma Docker ($($entry.Listener.Name)); nie rozpoznano kontenera - zakladam, ze to ta instalacja." "WARNING: port $($entry.Port) ($($entry.Name)) is held by Docker ($($entry.Listener.Name)); the container was not recognised - taking it for this installation's."))
                [void]$warnings.Add((UI-Text "Port $($entry.Port) jest zajety przez Dockera. Jesli serwer nie wstanie, sprawdz DIAGNOSTYKA i zatrzymaj inne instalacje." "Port $($entry.Port) is taken by Docker. If the server does not come up, check DIAGNOSTICS and stop the other installations."))
            }
            else {
                $who = if ($entry.Listener.Name) { (UI-Text "proces $($entry.Listener.Name), PID $($entry.Listener.Pid)" "the process $($entry.Listener.Name), PID $($entry.Listener.Pid)") } else { "PID $($entry.Listener.Pid)" }
                [void]$checks.Add((UI-Text "BŁĄD: port $($entry.Port) ($($entry.Name)) zajmuje $who." "ERROR: port $($entry.Port) ($($entry.Name)) is held by $who."))
                [void]$blocking.Add((Get-M2ProgramPortAdvice -Port ([int]$entry.Port) -Name ([string]$entry.Name) `
                    -ClientFixed ([bool]$entry.ClientFixed) -Key ([string]$entry.Key) -Listener $entry.Listener))
            }
        }
        if ($foreignHolders.Count -gt 0) {
            $projects = @($foreignHolders | ForEach-Object { $_.Project } | Select-Object -Unique)
            # Single quotes: PowerShell's tokenizer accepts the typographic
            # double quotes as string delimiters, so the pair around the
            # launcher's own menu entry ends a double-quoted string mid-sentence
            # and the whole module stops parsing.
            [void]$blocking.Add(
                ((UI-Text 'Porty serwera trzyma inna instalacja tego samego serwera (projekt: {0}). ' 'Another installation of the same server holds the server''s ports (project: {0}). ') +
                 (UI-Text 'Launcher zatrzymuje ją sam przy GRAJ i przy ZAINSTALUJ AKTUALIZACJE, a w wersji ' 'The launcher stops it by itself at PLAY and at CHECK FOR UPDATES, and in the ') +
                 (UI-Text 'konsolowej jest to opcja 21 (Zwolnij porty) - bez ruszania bazy, wolumenów i postępu. ' 'console launcher it is option 21 (Free the ports) - without touching the database, the volumes or the progress. ') +
                 (UI-Text 'Jeśli ten komunikat wraca mimo to, uruchom Docker Desktop i spróbuj ponownie. ' 'If this message comes back all the same, start Docker Desktop and try again. ') +
                 (UI-Text 'Samo wyłączenie Dockera nie pomaga: te kontenery mają politykę restart=unless-stopped, ' 'Just turning Docker off does not help: these containers have the restart=unless-stopped policy, ') +
                 (UI-Text 'więc wracają przy każdym starcie silnika i znów zajmują porty.' 'so they come back at every engine start and take the ports again.')) -f ($projects -join ', '))
        }
        # Kept for callers that only ask about the panel.
        $panelBusy = @($busy | Where-Object { [int]$_.Port -eq [int]$panelPort }) | Select-Object -First 1
        if ($panelBusy) { $portOwner = $panelBusy.Listener }
        $panelHolder = @($holders | Where-Object { $_.Ports -contains [int]$panelPort }) | Select-Object -First 1
        if ($panelHolder) { $dockerPortOwner = $panelHolder }
    }

    # Ports the stack binds on the host, against the ranges Windows reserved.
    # The game ports are the ones that fail in practice: a range that starts
    # at 11000 or 13000 blocks the login server or the channel, and nothing
    # in the launcher used to say so.
    # @(): a function's array of one unrolls to a bare object and of none to
    # $null, and under StrictMode neither has .Count - every player whose
    # Windows reserved exactly one port range (or none) got "The property
    # 'Count' cannot be found on this object" from Start, StartDocker and
    # Logs alike, because all three run this preflight.
    $excludedRanges = @(Get-M2ExcludedPortRanges)
    if ($excludedRanges.Count -gt 0) {
        $envPath = Join-Path $root 'linux-port\docker\.env'
        $authPort = 11000
        $dbPort = 3306
        if (Test-Path -LiteralPath $envPath -PathType Leaf) {
            $m = Select-String -LiteralPath $envPath -Pattern '^M2_AUTH_PORT=(\d+)$' | Select-Object -First 1
            if ($m) { $authPort = [int]$m.Matches[0].Groups[1].Value }
            $m = Select-String -LiteralPath $envPath -Pattern '^M2_DB_PUBLISH_PORT=(\d+)$' | Select-Object -First 1
            if ($m) { $dbPort = [int]$m.Matches[0].Groups[1].Value }
        }
        $stackPorts = @(
            @{ Port = $authPort; Name = (UI-Text 'serwer logowania' 'login server') },
            @{ Port = 13000; Name = (UI-Text 'kanal gry' 'game channel') },
            @{ Port = 13001; Name = (UI-Text 'kanal gry' 'game channel') },
            @{ Port = 13002; Name = (UI-Text 'kanal gry' 'game channel') },
            @{ Port = $dbPort; Name = (UI-Text 'baza danych' 'database') },
            @{ Port = [int]$panelPort; Name = 'panel' }
        )
        $hits = @()
        foreach ($p in $stackPorts) {
            $hit = Get-M2ExcludedPortHit -Port $p.Port -Ranges $excludedRanges
            if ($hit) { $hits += (UI-Text "$($p.Port) ($($p.Name), zakres $($hit.Start)-$($hit.End))" "$($p.Port) ($($p.Name), range $($hit.Start)-$($hit.End))") }
        }
        if ($hits.Count -gt 0) {
            [void]$checks.Add((UI-Text "BŁĄD: Windows zarezerwował porty serwera: $($hits -join ', ')." "ERROR: Windows has reserved server ports: $($hits -join ', ')."))
            [void]$blocking.Add((UI-Text 'Port serwera leży w zakresie zarezerwowanym przez Windows (Hyper-V/WSL), więc Docker nie może na nim nasłuchiwać. Uruchom PowerShell jako administrator: net stop winnat, kliknij GRAJ, a po starcie serwera: net start winnat. Zwykle pomaga też restart Windows.' 'A server port lies in a range Windows reserved (Hyper-V/WSL), so Docker cannot listen on it. Start PowerShell as administrator: net stop winnat, click PLAY, and once the server has started: net start winnat. A restart of Windows usually helps too.'))
        }
        else {
            [void]$checks.Add((UI-Text 'OK: żaden port serwera nie leży w zakresie zarezerwowanym przez Windows.' 'OK: no server port lies in a range Windows reserved.'))
        }
    }

    # The clock: a warning and never a stop. The build tolerates a clock that
    # runs behind where it cheaply can (the panel, the game's runtime stage),
    # and nothing in the running server needs the right hour - but a fresh
    # build of the game's libraries still asks apt, and the player is the only
    # one who can put the clock right.
    $clockSkew = Get-M2InternetClockSkew
    if ($null -ne $clockSkew) {
        if ([Math]::Abs($clockSkew) -le 300) {
            [void]$checks.Add((UI-Text 'OK: zegar Windows zgadza się z internetem.' 'OK: the Windows clock agrees with the Internet.'))
        }
        else {
            $which = $(if ($clockSkew -lt 0) { (UI-Text 'spóźnia się' 'is behind') } else { (UI-Text 'śpieszy się' 'is ahead') })
            [void]$checks.Add(((UI-Text 'UWAGA: zegar Windows {0} o {1} względem internetu.' 'WARNING: the Windows clock {0} by {1} against the Internet.') -f $which, (Get-M2ClockSkewText $clockSkew)))
            [void]$warnings.Add((UI-Text 'Zegar Windows jest przestawiony, a Docker bierze czas od Windows - budowa serwera może się zatrzymać na komunikacie „Release file ... is not valid yet”. Ustawienia → Czas i język → Data i godzina: włącz „Ustaw czas automatycznie”, sprawdź strefę czasową (dla Polski: Warszawa) i kliknij „Synchronizuj teraz”. Potem zamknij Docker Desktop (ikona w zasobniku → Quit) i kliknij GRAJ.' 'The Windows clock is wrong, and Docker takes its time from Windows - the server build can stop at the message "Release file ... is not valid yet". Settings > Time & language > Date & time: turn on "Set time automatically", check the time zone and click "Sync now". Then quit Docker Desktop (the tray icon > Quit) and click PLAY.'))
        }
    }
    if ($dockerEngineReady) {
        $dockerSkew = Get-M2DockerClockSkew
        if ($null -ne $dockerSkew -and [Math]::Abs($dockerSkew) -gt 300) {
            [void]$checks.Add(((UI-Text 'UWAGA: zegar Dockera odbiega od zegara Windows o {0}.' 'WARNING: Docker''s clock differs from the Windows clock by {0}.') -f (Get-M2ClockSkewText $dockerSkew)))
            [void]$warnings.Add((UI-Text 'Zegar maszyny Dockera rozjechał się z zegarem Windows (zdarza się po uśpieniu komputera). Zamknij Docker Desktop (ikona w zasobniku → Quit), uruchom go ponownie i kliknij GRAJ.' 'The Docker machine''s clock has drifted from the Windows clock (it happens after the computer sleeps). Quit Docker Desktop (the tray icon > Quit), start it again and click PLAY.'))
        }
    }

    # The disk under the engine: a write that must succeed, and the room the
    # Windows drive has left to grow it. The write stops the start - nothing
    # can be built or created on a read-only disk, and without this the
    # player downloads and applies the update first and learns it from an
    # "exit code: 255". The room is only a warning: a disk image that has
    # grown before carries free space of its own that no Windows number shows.
    if ($dockerEngineReady) {
        $diskFault = Get-M2DockerDiskFault
        if ($diskFault) {
            [void]$checks.Add((UI-Text "BŁĄD: dysk Dockera nie przyjmuje zapisu ($diskFault)." "ERROR: the Docker disk takes no writes ($diskFault)."))
            [void]$blocking.Add((UI-Text 'Dysk, na którym Docker Desktop trzyma obrazy i bazę świata (docker_data.vhdx), nie przyjmuje zapisu (read-only file system).' 'The disk where Docker Desktop keeps the images and the world''s database (docker_data.vhdx) takes no writes (read-only file system).') + [Environment]::NewLine + (Get-M2DockerDiskRemedy))
        }
        else {
            [void]$checks.Add((UI-Text 'OK: dysk Dockera przyjmuje zapis.' 'OK: the Docker disk takes writes.'))
        }
    }
    # A server folder OneDrive holds builds with files missing
    # (Get-M2OneDriveRootFor). Only a warning: a tree OneDrive has not turned
    # into placeholders yet still builds, and the move is the player's to make.
    $oneDrive = Get-M2OneDriveRootFor -Path $root
    if ($oneDrive) {
        [void]$checks.Add((UI-Text "UWAGA: folder serwera leży w OneDrive ($oneDrive)." "WARNING: the server folder is in OneDrive ($oneDrive)."))
        [void]$warnings.Add((Get-M2OneDriveRemedy -OneDriveRoot $oneDrive))
    }
    else {
        [void]$checks.Add((UI-Text 'OK: folder serwera nie leży w OneDrive.' 'OK: the server folder is not in OneDrive.'))
    }
    $dockerData = Get-M2DockerDataLocation
    if ($dockerData -and $null -ne $dockerData.FreeBytes) {
        $where = if ($dockerData.Drive) { $dockerData.Drive } else { $dockerData.Directory }
        if ($dockerData.FreeBytes -lt 15GB) {
            [void]$checks.Add(((UI-Text 'UWAGA: na dysku {0} zostało {1} wolnego miejsca, a tam Docker trzyma swój dysk.' 'WARNING: the drive {0} has {1} of free room left, and Docker keeps its disk there.') -f $where, (Format-M2Bytes $dockerData.FreeBytes)))
            [void]$warnings.Add(((UI-Text 'Na dysku {0} zostało tylko {1} wolnego miejsca. Docker trzyma tam swój dysk (docker_data.vhdx), który rośnie przy budowie serwera - świeża budowa potrzebuje ok. 15 GB. Gdy miejsca zabraknie w trakcie budowy, dysk Dockera przechodzi w tryb tylko do odczytu. Zwolnij miejsce przed kliknięciem GRAJ albo ZAINSTALUJ AKTUALIZACJE.' 'The drive {0} has only {1} of free room left. Docker keeps its disk there (docker_data.vhdx), which grows while the server builds - a fresh build needs about 15 GB. When the room runs out during a build, the Docker disk turns read-only. Free some room before you click PLAY or CHECK FOR UPDATES.') -f $where, (Format-M2Bytes $dockerData.FreeBytes)))
        }
        else {
            [void]$checks.Add(((UI-Text 'OK: na dysku {0} jest {1} wolnego miejsca dla Dockera.' 'OK: the drive {0} has {1} of free room for Docker.') -f $where, (Format-M2Bytes $dockerData.FreeBytes)))
        }
    }

    return [pscustomobject]@{
        CanStart = $blocking.Count -eq 0
        DockerCliPresent = $dockerCliPresent
        DockerProcessesRunning = $dockerProcessesRunning
        DockerEngineReady = $dockerEngineReady
        Virtualization = $virtualization
        Wsl = $wslState
        PanelPort = $panelPort
        CurrentProject = $currentProject
        ForeignPortHolders = @($foreignHolders)
        Checks = @($checks)
        Warnings = @($warnings)
        BlockingIssues = @($blocking)
    }
}

function Format-M2DockerPreflightReport {
    param([Parameter(Mandatory = $true)]$Report)

    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add((UI-Text '=== DIAGNOSTYKA METIN2 PLAYERBOTS ===' '=== METIN2 PLAYERBOTS DIAGNOSTICS ==='))
    foreach ($check in @($Report.Checks)) { $lines.Add([string]$check) }
    if (@($Report.Warnings).Count -gt 0) {
        $lines.Add('')
        $lines.Add((UI-Text 'Ostrzeżenia:' 'Warnings:'))
        foreach ($warning in @($Report.Warnings)) { $lines.Add("- $warning") }
    }
    if (@($Report.BlockingIssues).Count -gt 0) {
        $lines.Add('')
        $lines.Add((UI-Text 'Co trzeba zrobić:' 'What to do:'))
        foreach ($issue in @($Report.BlockingIssues)) { $lines.Add("- $issue") }
    }
    $lines.Add('')
    $lines.Add((UI-Text 'Wynik: ' 'Result: ') + $(if ($Report.CanStart) { (UI-Text 'można uruchomić serwer.' 'the server can be started.') } else { (UI-Text 'najpierw usuń powyższy problem.' 'fix the problem above first.') }))
    return $lines -join [Environment]::NewLine
}

Export-ModuleMember -Function @(
    'Get-M2LauncherErrorGuidance',
    'Get-M2OneDriveRootFor',
    'Get-M2OneDriveRemedy',
    'Get-M2DockerDiskRemedy',
    'Get-M2DockerDataLocation',
    'Get-M2DockerDiskFault',
    'Get-M2DiskSpaceReport',
    'Get-M2DockerPreflight',
    'Format-M2DockerPreflightReport',
    'Get-M2StackHostPorts',
    'Get-M2PublishedPortMatches',
    'Get-M2DockerPortHolders',
    'Get-M2ForeignPortHolders',
    'Stop-M2ForeignPortHolders',
    'Get-M2ProgramPortAdvice',
    'Get-M2ProgramPortConflicts'
)
