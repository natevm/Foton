import os

# For importing Vinyl.dll/so
os.environ["PATH"] += os.path.dirname(os.path.realpath(__file__)) + "../" + os.pathsep 

import Vinyl.Libraries as Libraries
import Vinyl.Systems as Systems
import Vinyl.Components as Components

__all__ = ["Libraries", "Systems", "Components"]