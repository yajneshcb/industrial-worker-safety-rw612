FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PORT=3000
WORKDIR /app

COPY requirements.txt ./requirements.txt
RUN pip install --no-cache-dir -r requirements.txt
COPY . .

EXPOSE 3000
CMD ["sh", "-c", "gunicorn --worker-class gthread --workers 1 --threads 8 --bind 0.0.0.0:${PORT:-3000} --access-logfile - --error-logfile - dashboard_app:app"]
