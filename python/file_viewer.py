import os


class FileViewer(object):
    def __init__(self):
        self.__curPath = ""
        self.__id2path = {}

    def open_dir(self, path):
        if not os.path.isdir(path):
            return []

        i = 0
        self.__curPath = os.path.abspath(path)
        all_files = os.listdir(path)
        for f in all_files:
            self.__id2path[i] = os.path.join(self.__curPath, f)
            i += 1

        return all_files

    def get_path_by_id(self, id):
        if self.__id2path.has_key(id):
            return self.__id2path[id]
        return ""


def view_file(fv, path):

    while len(path) > 0:
        files = fv.open_dir(path)
        counter = 1
        message = ""

        if os.path.exists(path):
            print "\ncurrent directory:" + os.path.abspath(path) + "\n"

        for f in files:
            if counter % 9 == 0:
                message += "\n"
            message += "  " + str(counter) + ":" + f
            counter += 1

        print message

        user_input = raw_input("\n select file or dir to open(input 0 to exit):")
        try:
            counter = int(user_input)
        except TypeError:
            break

        if counter == 0:
            break

        next_path = fv.get_path_by_id(counter - 1)
        if os.path.isfile(next_path):
            print "\n file to open:" + path + "\n"
            hf = open(next_path, "r")
            print hf.read()
            hf.close()
        else:
            path = next_path


filev = FileViewer()
view_file(filev, ".")





