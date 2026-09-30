"""Verify the original HUD updater wiring."""
from pathlib import Path
root=Path(__file__).parents[1]
source=(root/'src/update_service.cpp').read_text()
assert 'api.github.com/repos/OTPR26/twilight-hd-hud/releases/latest' in source
for name in ('Twilight-HD-HUD.dusk','Twilight-HD-HUD-tvOS.dusk'): assert name in source
assert 'update_policy::release' in source and 'update_policy::sha256' in source
assert 'State::AwaitingChoice' in source and 'desc.on_dismiss = dismiss_update' in source
assert 'IMPORT_OPTIONAL_SERVICE(HttpService, svc_http)' in (root/'src/mod.cpp').read_text()
assert 'src/update_service.cpp' in (root/'CMakeLists.txt').read_text()
assert 'Check Now' in (root/'src/ui.cpp').read_text()
print('PASS: independent updater and verified release selection')
