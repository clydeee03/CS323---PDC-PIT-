# Multiplayer Setup Quick Guide

## Same-LAN Checklist
- Both devices connected to same router/subnet
- Host firewall allows UDP port
- Client uses host IPv4 address (not localhost)
- Same executable version on host and clients
- Same `--port` and `--players` values

## Example Commands
Host:
```powershell
.\distributed_tetris.exe --host --port 7777 --players 2
```

Client:
```powershell
.\distributed_tetris.exe --client --address 192.168.1.42 --port 7777 --players 2
```

## Troubleshooting
- If client cannot connect: verify firewall rule and IP address
- If match does not start: ensure all expected players joined (`--players`)
- If lag/stall appears: use wired LAN or lower packet loss; lockstep waits for all inputs
