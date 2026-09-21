from typing import Annotated

from pydantic import Field

Uint8 = Annotated[int, Field(ge=0, le=255)]
Uint16 = Annotated[int, Field(ge=0, le=65535)]
Uint32 = Annotated[int, Field(ge=0, le=4294967295)]
Int16 = Annotated[int, Field(ge=-32768, le=32767)]
