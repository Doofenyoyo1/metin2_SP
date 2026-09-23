import app
import net
import ui
import snd
import wndMgr
import uiScriptLocale
import localeInfo
import time


app.SetGuildMarkPath("test")


class LogoWindow(ui.ScriptWindow):

    videoList = []

    def __init__(self, stream):
        ui.ScriptWindow.__init__(self)
        net.SetPhaseWindow(net.PHASE_WINDOW_LOGO, self)

        self.stream = stream
        self.playingVideo = 0
        self.bNeedUpdate = True
        self.preloadStarted = False
        self.preloadDone = False

        self.videoList = ["loading.avi"]

        self.preloadFiles = []
        for i in xrange(1, 33):
            self.preloadFiles.append(
                "locale/pl/ui/animated/login_big_%02d.jpg" % i
            )

        self.__Log("[INIT] Memory test - 32 JPG files prepared")

    def __del__(self):
        self.__Log("[DELETE] LogoWindow")
        ui.ScriptWindow.__del__(self)
        net.SetPhaseWindow(net.PHASE_WINDOW_LOGO, 0)

    def __Log(self, text):
        try:
            f = open("login_preload.log", "a")
            f.write(text + "\n")
            f.close()
        except:
            pass

    def Open(self):
        self.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
        self.SetWindowName("SelectLogoWindow")
        self.Show()

        self.__Log("[OPEN] Starting loading.avi")

        self.playingVideo = app.OnLogoOpen("loading.avi")

        self.__Log(
            "[VIDEO] OnLogoOpen result=%s" % self.playingVideo
        )

        app.ShowCursor()

    def Close(self):
        self.__Log("[CLOSE] LogoWindow")

        if self.playingVideo:
            app.OnLogoClose()
            self.playingVideo = 0

        self.KillFocus()
        self.Hide()
        app.HideCursor()

    def OnUpdate(self):
        if not self.bNeedUpdate:
            return

        # Let the AVI update normally until the preload test begins.
        if self.playingVideo:
            self.playingVideo = app.OnLogoUpdate()

        if not self.preloadStarted:
            self.preloadStarted = True
            # Disabled: this synchronously loads+frees 32 full-HD JPEGs
            # in a single blocking loop on the logo/intro screen, which
            # is exactly the ~8-10s stall seen before the login window
            # ever appears. Re-enable only for one-off memory testing.
            # self.__PreloadAndReleaseAll()

        # Never loop the AVI.
        if not self.playingVideo:
            self.__Log("[VIDEO] AVI finished")
            self.bNeedUpdate = False

            self.__Log("[LOGO] SetLoginPhase")
            self.stream.SetLoginPhase()

    def OnRender(self):
        if self.playingVideo:
            app.OnLogoRender()

    def __PreloadAndReleaseAll(self):
        self.__Log("[PRELOAD] START - load and immediately release each ImageBox")

        startTime = time.clock()
        loaded = 0

        for i in xrange(len(self.preloadFiles)):
            filename = self.preloadFiles[i]

            self.__Log(
                "[PRELOAD] %02d/32 START: %s"
                % (i + 1, filename)
            )

            imageBox = None

            try:
                imageBox = ui.ImageBox()
                imageBox.SetParent(self)
                imageBox.SetPosition(-10000, -10000)

                imageBox.LoadImage(filename)

                loaded += 1

                self.__Log(
                    "[PRELOAD] %02d/32 OK - releasing ImageBox"
                    % (i + 1)
                )

            except:
                self.__Log(
                    "[PRELOAD] %02d/32 FAILED"
                    % (i + 1)
                )

            # Release the ImageBox immediately.
            imageBox = None

        elapsed = time.clock() - startTime

        self.__Log(
            "[PRELOAD] FINISHED: %d/32 in %.3f sec"
            % (loaded, elapsed)
        )

        if loaded == 32:
            self.preloadDone = True
            self.__Log("[PRELOAD] COMPLETE")
        else:
            self.__Log("[PRELOAD] INCOMPLETE")

    def LoadNextVideo(self):
        if not self.playingVideo:
            self.playingVideo = app.OnLogoOpen("loading.avi")

    def CloseVideo(self):
        if self.playingVideo:
            app.OnLogoClose()
            self.playingVideo = 0
