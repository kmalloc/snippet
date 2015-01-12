from Tkinter import *


class App(Frame):
    __exit = None
    __label = None

    def __init__(self, master=None):
        Frame.__init__(self, master)
        self.pack()
        self.createWidgets()

    def createWidgets(self):
        self.__label = Label(self, text="Hello world")
        self.__label.pack()
        self.__exit = Button(self, text="Quit", command=self.quit)
        self.__exit.pack()


app = App()
app.master.title("Hello world")
app.mainloop()