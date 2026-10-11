
void signup(){
    fstream cekfile("game1.dat");//check if file already exist
    if(cekfile){
        cout<<"You already Sign-Up";
        Sleep(2000);
        return;
    }

    string user,pin;
    cls;
    showTitle();
    cout<<"Sign-Up"<<'\n';
    cout<<"Username (max 8 character):";
    cin>>user;user=enkrip(user);
    cout<<"Enter Pin(max 4 character):";
    cin>>pin;pin=enkrip(pin);

    fstream file("game1.dat",ios::out);
    if(!file.is_open()) {
        cout<<"Error open file1";
        exit(2);
    }
    file<<user+"\n";
    file<<pin;
    file.close();
}
bool signin(){
    string user1,pin1,user2,pin2;
    fstream file("game1.dat",ios::in);
    if(!file.is_open()){
        cout<<"Please Sign-Up first!";Sleep(2000);return 1;
    }
    getline(file,user1);user1=dekrip(user1);
    getline(file,pin1);pin1=dekrip(pin1);
    file.close();

    cls;
    showTitle();
    cout<<"Sign-In"<<'\n';
    cout<<"Username :";
    cin>>user2;
    //if(user2=="ben") {PlaySound("welcome.wav",NULL,SND_SYNC);return 0; } //this option has been replaced by vip menu

    cout<<"Enter Pin:";
    mASKED(pin2);
    //cin>>pin2;
    if(user1==user2 and pin1==pin2) {
        return 0;
    }
    else return 1;
}

