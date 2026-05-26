from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    mqtt_broker_host: str = "localhost"
    mqtt_broker_port: int = 1883
    db_path: str = "tempio.db"
    api_key: str = ""  # 빈 문자열이면 인증 안 함

    class Config:
        env_prefix = "TEMPIO_"
        env_file = ".env"
