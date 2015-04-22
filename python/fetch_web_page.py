import urllib2

proxy = urllib2.ProxyHandler({"http":"http://proxy2-as.asml.com:8080"})
opener = urllib2.build_opener(proxy)
urllib2.install_opener(opener)

req = urllib2.Request("http://www.cnblogs.com/catch")
response = urllib2.urlopen(req)
page = response.read()

print page
