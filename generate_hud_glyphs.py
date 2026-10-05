"""Generate the fixed UI glyph subset from a locally provided Noto CJK font."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

labels = "面板使控制中心温度性能内存已用可用总量占率负载历史采样点默认标准省电高未知模式频处理器图形溫內總負載歷採樣點預設標準電游戏加速开启关闭遊戲開啟關閉"
font = ImageFont.truetype(sys.argv[1], 15)
lines = ["/* Fixed UI labels only; rasterized from Noto Sans CJK. */", "static const struct { unsigned code; unsigned char alpha[256]; } hud_cjk[] = {"]
for char in sorted(set(labels)):
    image = Image.new("L", (16, 16))
    ImageDraw.Draw(image).text((0, 14), char, font=font, fill=255, anchor="ls")
    lines.append("{%d,{%s}}," % (ord(char), ",".join(map(str, image.tobytes()))))
lines.append("};")
Path("hud_cjk.h").write_text("\n".join(lines) + "\n")
