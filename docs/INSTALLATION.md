# Install KVM-Win on Windows

KVM-Win uses one Host PC and one or more Agent PCs. Download both files from the
same GitHub Release (or the same successful Windows Actions artifact):

- `KVM-Win-Setup-X.Y.Z.exe` — install this on the PC with the physical keyboard.
- `KVM-Agent-Setup-X.Y.Z.exe` — install this on each remote PC.

These are separate per-user installers. They include the matching x64 application,
Qt 6 libraries/plugins, OpenSSL, app-local Microsoft C/C++ runtime files,
third-party notices/licenses and uninstaller. You do not need Visual Studio,
Qt, vcpkg or a terminal. The current release workflow creates files with version
`0.1.0`; replace X.Y.Z with the actual file version shown in the Release page.

## Verify the downloads

Download `SHA256SUMS` beside both installers. In Windows PowerShell, run:

```powershell
Get-FileHash .\KVM-Win-Setup-X.Y.Z.exe -Algorithm SHA256
Get-FileHash .\KVM-Agent-Setup-X.Y.Z.exe -Algorithm SHA256
```

Compare each `Hash` value with the matching installer line in `SHA256SUMS`.
Use only installers and checksums downloaded together from the trusted project
release. A checksum detects accidental or in-transit modification; it does not
establish the publisher identity. Unsigned installers show Windows' unknown
publisher warning. Do not continue unless you trust the source and expected this
unsigned development release.

## Install and pair the PCs

1. On the keyboard PC, double-click `KVM-Win-Setup-X.Y.Z.exe`. Choose an install
   folder if desired, and optionally select a desktop shortcut. A standard user
   can install it without administrator access. Keep the default private network
   protections in Windows Firewall.
2. On every remote PC, double-click `KVM-Agent-Setup-X.Y.Z.exe` and install it.
3. Open **Start → KVM-Win Host → KVM-Win Host (main PC)** on the keyboard PC.
   Open **Start → KVM-Win Agent → KVM-Win Agent (remote PC)** on each remote PC.
   You can instead double-click the Host or Agent desktop shortcut.
4. On the Agent, note its full SHA-256 fingerprint. Copy it to Host's Devices
   page on the keyboard PC. On Host's Dashboard, note its fingerprint; copy that
   to the Agent's **Authorized host** field. Compare both 64-character
   fingerprints directly on the PCs, or through another trusted channel. Never
   trust a changed fingerprint based only on a network message.
5. On the Agent choose its own private LAN IPv4 address and port `24870`, then
   click **Save and start listening**. Ask the network administrator before using
   a different port or changing firewall policy.
6. On Host enter a recognizable PC name, that Agent's private IP, port `24870`
   and its verified Agent fingerprint. Click **Pair and connect**. Repeat for up
   to five Agent PCs. Automatic network discovery is not implemented.
7. Keep a simple application (such as Notepad) open on each PC while validating.
   Select and check the connected Agents that should receive mirrored keys, then
   click **Enable keyboard MIRROR**. Host keystrokes continue locally and go to
   each selected Agent's focused application. Click **Disable keyboard MIRROR**
   or **Emergency stop** to return to Local. `Ctrl+Alt+M` toggles mirroring;
   `Ctrl+Alt+Esc` is the emergency shortcut. Remote control is exclusive.
   Matching keyboard layouts are required. Mouse mirroring is a separate opt-in.

## Windows Firewall and connection requirements

The Agent listens for **inbound TCP on its selected private IPv4 address, port
24870 by default**. The Host initiates an **outbound TCP connection** to that
address and port. Both PCs must have LAN reachability and the same permitted
private-network profile. Each PC needs an interactive logged-in Windows session;
this is not a service that injects input at the login screen.

If Windows Firewall prompts while the Agent starts listening, allow the Agent
only on the trusted **Private networks** profile if your organization's policy
permits it. Do not enable Public network access, turn the firewall off or create
an all-ports rule. The installer creates no firewall exception. Managed devices
may need their administrator to allow inbound TCP 24870 from the host/subnet.
Never forward the Agent port from the Internet. Connections use mutually pinned
TLS certificates; no cloud service or internet listener is used.

## Startup, updates and removal

By default, closing a window exits it; Host returns to Local before closing.
You can explicitly select close-to-tray or login startup in each app. Agent login
startup only starts its saved listener configuration. Keep each app installed in
a distinct folder. Upgrading a role replaces that role's program files and keeps
its pairing, identity, settings and optional startup choice. Do not manually
delete the ShareKeysHost or ShareKeysAgent folders under `%LOCALAPPDATA%` if you
want to preserve those settings.

Use **Start → KVM-Win Host → Uninstall KVM-Win Host** to remove Host. Use
**Start → KVM-Win Agent → Uninstall KVM-Win Agent** to remove Agent. Uninstall
removes that role's files, shortcuts and its startup registry value; it retains
the user's identity and settings so an upgrade/reinstall does not require pairing
again. Remove retained settings manually only when intentionally revoking and
recreating that role's identity. If migrating from the old combined ShareKeys
installer, first stop both apps and uninstall that old combined package; its
uninstaller retains the same per-user settings. Then install the separate roles.

## Troubleshooting

- **Agent shows listening but Host is offline:** confirm both PCs' private LAN
  address, the Agent's selected interface, port, private-profile firewall rule,
  and the two independently checked fingerprints. A DHCP address change requires
  updating the Host's saved address.
- **Wrong fingerprint or pairing rejected:** stop before enabling input. Copy and
  compare the complete fingerprints again. A new identity requires deliberate
  re-pairing; do not accept an unexpected change.
- **The installed app cannot launch:** keep the complete installation directory;
  its Qt plugins and DLLs are required. Reinstall from the matching x64 package.
- **Remote input does not reach an application:** focus that application's window
  on the Agent. Use equal keyboard layouts and ordinary applications with equal
  privilege. Secure desktops, login screens, Ctrl+Alt+Delete and elevation
  boundaries are unsupported.
- **Stop immediately:** use Host's red emergency control or `Ctrl+Alt+Esc`.
  Do not infer that the destination application acted on input from a green
  connection indicator or network round-trip time.

This repository revision has not been exercised on a clean Windows VM. Verify the
release notes and Windows test record before relying on it for important work.
