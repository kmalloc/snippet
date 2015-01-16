import wx


class MyFrame(wx.Frame):
    def __init__(self, parent, title):
        wx.Frame.__init__(self, parent, title=title, size=(200, 100))
        self.control = wx.TextCtrl(self, style=wx.TE_MULTILINE)
        self.Show(True)

import sys
print sys.argv

fdd = open(sys.argv[0])
print "\ncontent of current file:\n"
print fdd.read()

app = wx.App(False)
frame = MyFrame(None, "Hello GUI")
app.MainLoop()

