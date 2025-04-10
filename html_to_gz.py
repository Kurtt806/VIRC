import gzip
import shutil
from pathlib import Path

# Đường dẫn đến file HTML nguồn và thư mục đích
source_file = Path("index.html")
#source_file = Path("index_relayout.html")
# source_file = Path("index_full_flash_info.html")

output_dir = Path("data")
output_file = output_dir / "index.html.gz"

# Tạo thư mục data nếu chưa có
output_dir.mkdir(exist_ok=True)

# Kiểm tra file gốc tồn tại
if not source_file.exists():
    print("❌ index.html không tồn tại.")
else:
    # Thực hiện nén
    with open(source_file, 'rb') as f_in:
        with gzip.open(output_file, 'wb') as f_out:
            shutil.copyfileobj(f_in, f_out)
    print("✅ Đã nén xong: index.html.gz trong thư mục 'data'")
