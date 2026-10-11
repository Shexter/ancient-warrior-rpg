void training(string heroName,int heroMaxMP,int heroMP,int heroMaxHP,int &heroHP,int heroDMG,int heroDEF,int heroLIFE,int &heroLVL,int &heroEXP,int &heroTNL,int heroPath,int &heroGold,int inventoryID[],int inventoryQTY[],int itemID[],string itemName[],int dropRate[]){
    if(heroHP==0){
        warningMsg("Fallen Hero, go revive your self!!");
        return;
    }
    string monsterName[10]={"Rabbit","Fox","Bat","Horse","Bull","Bear","Snake","Crocodile","Rhino","Lion"};
    cls;
    showTitle();
    int caveLvl,round;
    cout<<"Training Cave is a Non Magical Ground!\n";
    cout<<"Cave Level: ";
    cin>>caveLvl;
    cout<<"Rounds: ";
    cin>>round;
    if(caveLvl<1 or caveLvl>30){
        cout<<"Available Cave is 1 to 30";
        Sleep(2000);
        return;
    }
    if(round<0){
        cout<<"Invalid rounds...";
        Sleep(2000);
        return;
    }
    for(int i=0;i<round;i++){
        cls;
        int monsterHP=(100*caveLvl); //rand()%10*caveLvl
        int monsterMaxHP=monsterHP;
        int monsterDMG=(rand()%5+15)*2*caveLvl;  //20-280
        int monsterDEF=(rand()%5+5)*caveLvl;  //5-90

        int gainGold=(rand()%10+10)*10*caveLvl; //100-1900
        int gainExp=100*caveLvl;
        cout<<heroName<<" is training..."<<'\n';
        while(1){
            cout<<"\r"<<heroName<<"<:"<<heroHP;hpmpBar(heroHP,heroMaxHP);
            cout<<"---vs---";hpmpBar(monsterHP,monsterMaxHP);
            cout<<monsterHP<<":>"<<monsterName[caveLvl/10+1]<<"     ";

            int damage=zeroint(heroDMG-monsterDEF); //dmg hero dikurangi armor monster
            monsterHP-=damage;monsterHP=zeroint(monsterHP);
            if(monsterHP==0) {
                break;
            }
            int monsDMG=zeroint(monsterDMG-heroDEF); //dmg monst dikurangi armor hero
            heroHP-=monsDMG;
            if(heroHP<=0) {
                cout<<'\n'<<heroName<<" has been injured badly";Sleep(2000);
                heroHP=0;
                heroEXP-=gainExp;
                if(heroEXP<0) heroEXP=0;
                return;
            }
            else {
                int recoverHp;
                if(heroLIFE>heroMaxHP-heroHP) recoverHp=heroMaxHP-heroHP;
                else recoverHp=heroLIFE;
                heroHP+=recoverHp;
                //cout<<"Recovered "<<recoverHp<<" health";
            }
            Sleep(1000);
        }

        cout<<"\nTraining is over..."<<'\n';
        cout<<heroName<<" gained..."<<gainExp<<" point exp"<<'\n';
        cout<<"Gold collected..."<<gainGold<<'\n';
        heroGold+=gainGold;
        heroEXP+=gainExp;
        levelUP(gainExp,heroTNL,heroLVL,heroPath);

        int dropID=rndDrop(itemID,dropRate,caveLvl);

        if(dropID!=0) {
            cout<<"DROP Item : "<<id2Name(dropID,itemID,itemName);
            updBag(dropID,inventoryID,inventoryQTY,1);
        }
        Sleep(1000);
    }

}
