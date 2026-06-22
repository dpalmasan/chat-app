FROM python:3.14-slim

WORKDIR /app
COPY clients/web_client/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY clients/web_client .

EXPOSE 5000
ENTRYPOINT ["python", "app.py"]
