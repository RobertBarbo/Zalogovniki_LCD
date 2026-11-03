"""Run the asset generator before PlatformIO scans source dependencies."""
from pathlib import Path
import runpy

Import('env')
script = Path(env.subst('$PROJECT_DIR')) / 'scripts' / 'embed_assets.py'
runpy.run_path(str(script), run_name='__main__')
