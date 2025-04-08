This is a sample program to get an access token via Windows' Web Account Manager (WAM) service.

The main purpose of this program is to detect & isolate issues at WAM level instead of other programs that depend on WAM such as Microsoft Office clients.  

## Download
You can download the latest build from [here](https://github.com/jpmessaging/GetToken/releases/download/v0.6.0/GetToken.zip)

## Usage
While you can run this program without any options, this program supports input options to customize the requests for the IdP and progam behavior. You can see available options by using `--help`, `-h`, or `-?`:  

For example:

    C:\>GetToken.exe -?
    GetToken (version 0.6.0)
    Available options:
      -h, --help           Show this help message
      -?                   Show this help message
      -v, --version        Show version
      -c, --clientid arg   Client ID. Default: d3590ed6-52b3-4102-aeff-aad2292ab01c
      --scopes arg         Scopes of the token (e.g., "https://outlook.office365.com//.default offline_access openid profile")
      -p, --property arg   Request property (e.g., login_hint=user01@example.com, prompt=login). Can be used multiple times
      --showaccountsonly   Show Web Accounts and exit
      --showtoken          Show Access Token
      --signout            Sign out of Web Accounts
      -t, --tracepath arg  Folder path for a trace file
      -n, --notrace        Disable trace
      -w, --wait           Wait execution until user enters
      --wamcompat          Add "wam_compat=2.0" to WebTokenRequest
      --claimcapability    Add claims client capability "cp1" to request: claims={"access_token":{"xms_cc":{"values":["CP1"]}}}

    Note: All options are case insensitive.

    Example 1: GetToken.exe -p resource=https://outlook.office365.com/
    Run with default configurations for the specified resource

    Example 2: GetToken.exe -p resource=https://outlook.office365.com/ --claimcapability
    Add claim capability to the request

    Example 3: GetToken.exe -p resource=https://outlook.office365.com/ --claimcapability -p login_hint=user01@example.com -p msafed=0
    Add the given properties to the request

    Example 4: GetToken.exe -p resource=https://outlook.office365.com/ --scopes open_id profiles
    Use the given scopes for the token

    Example 5: GetToken.exe -p resource=https://outlook.office365.com/ --signout
    Sign out from all web accounts before making token requests

## License
Copyright (c) 2024 Ryusuke Fujita

This software is released under the MIT License.  
http://opensource.org/licenses/mit-license.php

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---
This software uses the following projects:  

https://github.com/badaix/popl

MIT License

Copyright (c) 2015-2016 Johannes Pohl

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---
https://github.com/tobiaslocker/base64

MIT License

Copyright (c) 2019 Tobias Locker

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.