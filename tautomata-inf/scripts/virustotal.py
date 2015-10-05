# Virustotal API class
# See API docs @ http://www.virustotal.com/advanced.html for the API reference.
# Usage:
#  api = VirusTotalAPI(my_api_key)
# API methods can be called like python functions:
#  data = api.get_file_report(resource=RESOURCE_ID)
# API methods return python objects (Or an exception)
#  print data["permalink"]
# All the API functions take the API arguments as their kwargs.
# The API key is automatically sent with the 
#
# Uploading a file requires the 'posthandler' module - if it is not
# available then the scan_file function will not work, but the rest
# of the API class will work.

import simplejson
import urllib
import urllib2
import functools
import urlparse

try:
    import posthandler
    post_opener = urllib2.build_opener(posthandler.MultipartPostHandler)
except ImportError:
    posthandler = None
    
    
class ModuleNotFound(Exception):
    ''' Module has not been found '''


class VirusTotalAPI(object):
    api_url = "https://www.virustotal.com/api/"
    api_methods = ["get_file_report","get_url_report",
                   "scan_url","make_comment"]  # Generic dynamic property methods (_call_api)
    special_methods = ["scan_file"] # Methods with their own function
    
    def __init__(self, api_key):
        self.api_key = api_key
        for method in self.api_methods:
            setattr(self, method, functools.partial(self._call_api,method,key=self.api_key))
        for smethod in self.special_methods:
            setattr(self, smethod, functools.partial(getattr(self,"_special_"+smethod),key=self.api_key))
    
    def _call_api(self, function, **kwargs):
        url = self.api_url + function + ".json"
        data = urllib.urlencode(kwargs)
        req = urllib2.Request(url,data)
        returned = urllib2.urlopen(req).read()
        return simplejson.loads(returned)
    
    def _special_scan_file(self, **kwargs):
        if not posthandler:
            raise ModuleNotFound("posthandler module needed to submit files")
        json = post_opener.open(self.api_url + "scan_file.json",kwargs).read()
        return simplejson.loads(json)
