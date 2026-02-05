# ripme
A kernel webserver that lets you control the RIP register

https://github.com/user-attachments/assets/a9cbc91b-ee7b-4eb4-9c37-d7176ee548e7

Worse Avaliability on a webserver ever! Includes a power cycle button as well if the blue screen does not auto reboot. 

# How to self host
Warning: this is a mess of python scripts and requires a second windows computer to be up at all times. It works for me! It probably won't for you. 


1. Prepare target vm
Attach to a seperate machine for debugging. Without this the driver will not load, even if self signing options are enabled. Not sure why. More info at the link:
https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/setting-up-a-network-debugging-connection

All you need to do is enter:
```
bcdedit /debug on
bcdedit /dbgsettings net hostip:192.168.1.100 port:50000
```
On the other windows machine on your network, open windbg and paste in the key you got from this step. To test if this worked, type in:
```
r rip=808080
```
to bluescreen.

Now, place the .sys driver or somewhere on the victim machine. Then create a service and start it. 
```
sc.exe create servicename binpath="C:\VulnerableDriver.sys" type=kernel
sc.exe start servicename
```
Later, start it on boot. While editing, do not do this as it is easy to mess up the escape functionality, and then the only way to escape is to reboot. 

The webserver should be running! Go to port 80 of the target machine and ensure it is up. 

2. Set up proxy and target VM. 

Add this flag to vnc. This will stream your vnc session with no password and will accept input to your LAN. 
`args: -vnc 0.0.0.0:77,password=off`

Now, use a second python script called d.py which will proxy vnc and remove the input. Edit it to set the ip address of the vnc server to your proxmox ip address. It will take in input on port 5977 (where vnc is running) and output with dropping inputs on port 599. 
```
python3 d.py
```
On the same computer, clone noVNC from https://github.com/novnc/noVNC
```
noVNC/utils/novnc_proxy --vnc localhost:5999 --listen 6700
```
This should open up a new webpage on port 6700. You should be to connect on http://proxyip:6700/vnc.html?autoconnect=true&reconnect=true&viewonly=true
Now keep in mind this will also open up anything in your vnc directory to your lan, and to the world. I added restrictions in my reverse proxy configuration to only allow the vnc.html page. 

3. Add a power button
Run (and edit) this python script called reset.py. Edit it to adjust the vm id to be correct. Also change the secret key if you want, its not to important however. More there to ensure random scanning on a LAN does not result in a power cycle. Run this script as root on the proxmox server.
```
python3 reset.py
```
It should be good to go! Now create your webpage where everything is held together. 

4. Self host with https
The issue with putting everything in iframes is that everything requires its own new domain name. I assigned subdomains to the novnc proxy host, the main webpage, and the driver webpage. Do this as well. Unless you want to keep this entirely on LAN.

Adjust the html under the webpage folder. Switch the iframes to either use the http websites, or switch everything to https. Also, adjust the power.php script. Now, place these files in /var/www/html and self host!
Everything should work!
