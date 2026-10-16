
#!/bin/bash

sudo -u postgres psql -p 5434 -c "TRUNCATE TABLE products RESTART IDENTITY;"
